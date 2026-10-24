"""Exercise final publication decisions using synthetic archives and a fake API."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools"))
MODULE = ROOT / "Tools/publish_verified_release.py"
spec = importlib.util.spec_from_file_location("publication_under_test", MODULE)
pub = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pub)


class FakeGitHub:
    def __init__(self, baseline, draft):
        self.baseline, self.draft = baseline, draft
        self.calls, self.writes = [], []
        self.uncertain = False
        self.wrong_tag = False
        self.latest_calls = 0
        self.change_baseline = False

    def request(self, route, payload=None):
        self.calls.append((route, payload))
        if payload is not None:
            self.writes.append((route, payload))
            if route != "releases/52" or payload != {"draft":False, "make_latest":"true"}:
                raise AssertionError("Unexpected remote mutation")
            self.draft["draft"] = False
            if self.uncertain:
                raise RuntimeError("Simulated lost response after publish")
            return copy.deepcopy(self.draft)
        if route == "releases/latest":
            self.latest_calls += 1
            if self.change_baseline and self.latest_calls > 1:
                return dict(self.baseline, id=999)
            return copy.deepcopy(self.baseline if self.draft["draft"] else self.draft)
        if route == "releases/51": return copy.deepcopy(self.baseline)
        if route == "releases/52": return copy.deepcopy(self.draft)
        if route.startswith("git/matching-refs/tags/"):
            if self.draft["draft"] and not self.wrong_tag: return []
            return [dict(ref="refs/tags/" + self.draft["tag_name"], object=dict(type="commit", sha="d"*40 if self.wrong_tag else "a"*40))]
        if route == "git/commits/" + "a"*40: return dict(tree=dict(sha="b"*40))
        raise AssertionError("Unexpected API read: " + route)


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.version = "0.3.95-community-replays.52"
        self.old_version = "0.3.95-community-replays.51"
        self.names, self.old_names = pub.version_names(self.version), pub.version_names(self.old_version)
        self.player = self.root / "player"
        required = pub.verify.load_tool("publication_fixture_patch", "Build-UpdatePatch.py").REQUIRED
        self.new = {name:b"synthetic unchanged game data" for name in required}
        self.new["InitialDUnity.exe"] = b"new synthetic executable"
        self.new["InitialDUnity_Data/StreamingAssets/GUNSAI/data.bin"] = b"synthetic imported course"
        old = dict(self.new, **{"InitialDUnity.exe":b"old synthetic executable"})
        for name,data in self.new.items():
            path = self.player/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        self.inventory = pub.verify.candidate_inventory(self.player)
        self.old_paths = self.downloads(old, self.old_names)
        self.new_paths = self.downloads(self.new, self.names)
        self.source = self.root / "source"
        contents = {name:("$version='"+self.version+"';\n" if name=="Install-InitialD.ps1" else "synthetic setup "+name).encode() for name in pub.SETUP_FILES}
        for name,data in contents.items():
            if name.endswith((".ps1",".cmd")):
                path=self.source/"Tools"/name
            elif name=="Idas3UpdateInstaller.exe":
                path=self.source/"Assets/Resources/UpdateInstaller.exe.bytes"
            else: continue
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        self.setup = self.zip(self.names["setup"], contents)
        setup_report=self.write("candidate/setup.json",dict(passed=True,version=self.version,sha256=self.sha(self.setup),files={name:hashlib.sha256(data).hexdigest() for name,data in contents.items()}))
        candidate=self.write("candidate/summary.json",dict(checks=[dict(name="setup-package",report="setup.json",reportSha256=self.sha(setup_report))]))
        upgraded=self.write("upgrade/report.json",dict(targetArchives=[self.ref(path) for path in self.new_paths]))
        self.accepted=self.write("acceptance.json",dict(reports=dict(candidate=self.ref(candidate),upgrade=self.ref(upgraded))))
        self.receipt=dict(releaseReady=True,applicationVersion=self.version,sourceCommit="a"*40,candidateInventorySha256=self.inventory["inventorySha256"])
        records=[]; payload={}
        for name,data in self.new.items():
            changed=old.get(name)!=data
            records.append(dict(path=name,size=len(data),sha256=hashlib.sha256(data).hexdigest(),included=changed))
            if changed:payload[name]=data
        payload["update-patch.json"]=json.dumps(dict(schema=1,baseVersion=self.old_version,targetVersion=self.version,files=records)).encode()
        self.patch=self.zip("Initial-D-Update-from-"+self.old_version+"-to-"+self.version+"-Patch.zip",payload)
        archives=[*self.new_paths,self.setup,self.patch]
        checksums=self.root/"SHA256SUMS.txt";checksums.write_text(''.join(self.sha(path)+"  "+path.name+"\n" for path in archives))
        notes=self.root/"notes.md";notes.write_text("Synthetic release test.\n")
        self.plan_data=dict(schema=pub.SCHEMA,repository=pub.REPO,releaseId=52,baselineReleaseId=51,tag="v"+self.version,sourceCommit="a"*40,title="Synthetic .52",notes=self.ref(notes),acceptance=self.ref(self.accepted),
            assets=dict(core=self.ref(self.new_paths[0]),courses=self.ref(self.new_paths[1]),setup=self.ref(self.setup),checksums=self.ref(checksums),patches=[dict(self.ref(self.patch),baseVersion=self.old_version,baseDownloads=[self.ref(path) for path in self.old_paths])]))
        self.plan=self.write("publication.json",self.plan_data)
        baseline=dict(id=51,tag_name="v"+self.old_version,draft=False,prerelease=False,assets=[self.remote(path) for path in self.old_paths])
        draft=dict(id=52,tag_name="v"+self.version,target_commitish="a"*40,name="Synthetic .52",body=notes.read_text(),draft=True,prerelease=False,assets=[self.remote(path) for path in [*archives,checksums]],html_url="https://github.com/"+pub.REPO+"/releases/tag/v"+self.version)
        self.github=FakeGitHub(baseline,draft)
        for item in (patch.object(pub.gate,"check",return_value=self.receipt),patch.object(pub,"ROOT",self.source),patch.object(pub.verify,"git",return_value=("b"*40+"\n").encode())):
            item.start();self.addCleanup(item.stop)

    @staticmethod
    def sha(path):return pub.verify.sha256(path)
    def ref(self,path):return dict(path=str(path),sha256=self.sha(path))
    def write(self,name,data):
        path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(data));return path
    def zip(self,name,contents):
        path=self.root/name
        with zipfile.ZipFile(path,"w") as out:
            for key,data in contents.items():out.writestr(key,data)
        return path
    def downloads(self,data,names):
        courses={key:value for key,value in data.items() if "/GUNSAI/" in key}
        return [self.zip(names["core"],{key:value for key,value in data.items() if key not in courses}),self.zip(names["courses"],courses)]
    def remote(self,path):return dict(name=path.name,state="uploaded",size=path.stat().st_size,digest="sha256:"+self.sha(path))
    def run_mode(self,mode):
        report={"passed":False};history=[]
        pub.execute(self.plan,self.player,self.github,mode,report,lambda:history.append(copy.deepcopy(report)))
        return report,history

    def test_read_only_default_validates_everything_without_writing(self):
        report,_=self.run_mode("check")
        self.assertTrue(report["passed"]);self.assertFalse(report["published"]);self.assertFalse(self.github.writes)

    def test_publish_is_one_specific_mutation_after_acceptance(self):
        report,history=self.run_mode("publish")
        self.assertTrue(report["passed"] and report["published"])
        self.assertEqual(self.github.writes,[("releases/52",{"draft":False,"make_latest":"true"})])
        self.assertTrue(any(row.get("phase")=="publishing" and row.get("publicationRequested") for row in history))

    def test_incomplete_acceptance_prevents_even_remote_reads(self):
        with patch.object(pub.gate,"check",side_effect=ValueError("incomplete real-world acceptance")):
            with self.assertRaisesRegex(ValueError,"incomplete"):
                self.run_mode("publish")
        self.assertFalse(self.github.calls)

    def test_setup_cannot_point_to_the_previous_release(self):
        report=self.root/"candidate/setup.json";data=json.loads(report.read_text());data["version"]=self.old_version;report.write_text(json.dumps(data))
        candidate=self.root/"candidate/summary.json";data=json.loads(candidate.read_text());data["checks"][0]["reportSha256"]=self.sha(report);candidate.write_text(json.dumps(data))
        accepted=json.loads(self.accepted.read_text());accepted["reports"]["candidate"]=self.ref(candidate);self.accepted.write_text(json.dumps(accepted))
        self.plan_data["acceptance"]=self.ref(self.accepted);self.write("publication.json",self.plan_data)
        with self.assertRaisesRegex(ValueError,"pinned"):
            self.run_mode("publish")
        self.assertFalse(self.github.calls)

    def test_extra_rom_entry_rejected_before_compressed_payload_hash(self):
        with zipfile.ZipFile(self.new_paths[0],"a") as out:out.writestr("rom/README.txt","excluded instructions")
        real_sha=pub.verify.sha256
        def guarded(path):
            self.assertNotEqual(Path(path),self.new_paths[0],"Unexpected archive payload was hashed")
            return real_sha(path)
        with patch.object(pub.verify,"sha256",side_effect=guarded):
            with self.assertRaisesRegex(ValueError,"unexpected entries"):
                self.run_mode("publish")
        self.assertFalse(self.github.calls)

    def test_existing_patch_bases_cannot_be_dropped(self):
        self.github.baseline["assets"].append(dict(name="Initial-D-Update-from-0.3.95-community-replays.50-to-"+self.old_version+"-Patch.zip"))
        with self.assertRaisesRegex(ValueError,"omit supported"):
            self.run_mode("publish")
        self.assertFalse(self.github.writes)

    def test_patch_cannot_retain_a_changed_executable(self):
        old=pub.upgrade.archive_inventory(self.old_paths)
        with zipfile.ZipFile(self.patch) as archive:
            manifest=json.loads(archive.read("update-patch.json"))
        for row in manifest["files"]:row["included"]=False
        self.zip(self.patch.name,{"update-patch.json":json.dumps(manifest).encode()})
        with self.assertRaisesRegex(ValueError,"Retained patch file"):
            pub.verify_patch(self.patch,old,self.inventory["files"],self.old_version,self.version)

    def test_remote_wrong_assets_commit_notes_or_draft_state_block_publication(self):
        mutations=[lambda d:d["assets"][0].update(digest="sha256:"+"0"*64),lambda d:d["assets"].pop(),
                   lambda d:d["assets"].append(dict(name="upload-transfer.zip")),lambda d:d.update(target_commitish="main"),
                   lambda d:d.update(body="unreviewed notes"),lambda d:d.update(draft=False),lambda d:d.update(prerelease=True)]
        original=copy.deepcopy(self.github.draft)
        for mutate in mutations:
            self.github.draft=copy.deepcopy(original);mutate(self.github.draft)
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):self.run_mode("publish")
            self.assertFalse(self.github.writes)

    def test_existing_wrong_tag_cannot_be_published(self):
        self.github.wrong_tag=True
        with self.assertRaisesRegex(ValueError,"tag points"):
            self.run_mode("publish")
        self.assertFalse(self.github.writes)

    def test_checksum_table_cannot_omit_a_patch(self):
        path=self.root/"SHA256SUMS.txt";path.write_text('\n'.join(path.read_text().splitlines()[:-1])+'\n')
        self.plan_data["assets"]["checksums"]=self.ref(path);self.write("publication.json",self.plan_data)
        with self.assertRaisesRegex(ValueError,"Checksum file omits"):
            self.run_mode("publish")
        self.assertFalse(self.github.writes)

    def test_latest_change_before_publish_stops_the_write(self):
        self.github.change_baseline=True
        with self.assertRaisesRegex(ValueError,"Stable baseline changed"):
            self.run_mode("publish")
        self.assertFalse(self.github.writes)

    def test_acceptance_change_before_publish_stops_the_write(self):
        with patch.object(pub.gate,"check",side_effect=[self.receipt,dict(self.receipt,changed=True)]):
            with self.assertRaisesRegex(ValueError,"Acceptance changed"):
                self.run_mode("publish")
        self.assertFalse(self.github.writes)

    def test_lost_publish_response_has_a_read_only_recovery_path(self):
        self.github.uncertain=True
        report={"passed":False}
        with self.assertRaisesRegex(RuntimeError,"lost response"):
            pub.execute(self.plan,self.player,self.github,"publish",report,lambda:None)
        self.assertTrue(report["publicationRequested"]);self.assertFalse(report["passed"])
        self.assertEqual(len(self.github.writes),1)
        verified,_=self.run_mode("verify-published")
        self.assertTrue(verified["passed"] and verified["published"])
        self.assertEqual(len(self.github.writes),1)


if __name__=="__main__":unittest.main()
