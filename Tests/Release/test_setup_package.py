"""Prevent setup packages from silently substituting unverified recovery inputs."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('setup_package',ROOT/'Tools/Build-SetupPackage.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)


class SetupPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'Tools').mkdir()
        for name in package.SCRIPTS:(self.root/'Tools'/name).write_text("$version='0.1.0';\n" if name=='Install-InitialD.ps1' else 'inert script fixture')
        self.helper=self.root/'helper.exe';self.helper.write_bytes(b'inert helper fixture')
        self.archive=self.root/'setup.zip'
        h=package.sha(self.helper.read_bytes())
        self.normal={'passed':True,'helperSha256':h,'tests':self.case_rows(23,['process-handoff-restart','rollback-failure-keeps-recovery','session-inside-game-keeps-payloads']),'count':23}
        self.recovery={'passed':True,'helperSha256':h,'realInterruptionIncluded':True,'tests':self.case_rows(21,['interrupted-seed-updater','interrupted-recovery','running-game','missing-last-backup','locked-destination']),'count':21}
        self.launcher={'passed':True,'tests':self.case_rows(9,['cmd-single','missing-executable','damaged-backup','select-second','cancel-selection']),'count':9,
                       'inputs':{name:package.sha((self.root/'Tools'/name).read_bytes()) for name in ('Recover Initial D.cmd','Recover-InitialD.ps1')}}
        self.launcher['inputs']['Idas3UpdateInstaller.exe']=h
        self.paths=[self.root/name for name in ('normal.json','recovery.json','launcher.json')]

    @staticmethod
    def case_rows(count,required):
        return [{'name':n,'passed':True} for n in required+['fixture-'+str(i) for i in range(count-len(required))]]

    def build(self):
        for path,data in zip(self.paths,(self.normal,self.recovery,self.launcher)):path.write_text(json.dumps(data))
        with patch.object(package,'ROOT',self.root):return package.build(self.helper,self.archive,*self.paths)

    def test_verified_package_contains_exact_sources(self):
        report=self.build()
        with zipfile.ZipFile(self.archive) as archive:
            self.assertEqual(set(archive.namelist()),set(package.SCRIPTS)|{'README.txt','Idas3UpdateInstaller.exe'})
            for name in package.SCRIPTS:self.assertEqual(archive.read(name),(self.root/'Tools'/name).read_bytes())
        self.assertEqual(report['sha256'],package.sha(self.archive.read_bytes()))

    def test_changed_helper_is_refused(self):
        self.helper.write_bytes(b'unverified replacement')
        with self.assertRaises(ValueError):self.build()
        self.assertFalse(self.archive.exists())

    def test_changed_launcher_is_refused(self):
        (self.root/'Tools/Recover-InitialD.ps1').write_text('unverified change')
        with self.assertRaises(ValueError):self.build()
        self.assertFalse(self.archive.exists())

    def test_missing_real_interruption_is_refused(self):
        self.recovery['realInterruptionIncluded']=False
        with self.assertRaises(ValueError):self.build()

    def test_missing_or_failed_case_is_refused(self):
        self.recovery['tests'][0]['name']='not-the-required-case'
        with self.assertRaises(ValueError):self.build()
        self.recovery['tests'][0]={'name':'interrupted-seed-updater','passed':False}
        with self.assertRaises(ValueError):self.build()

    def test_existing_download_is_never_replaced(self):
        self.archive.write_bytes(b'existing download')
        with self.assertRaises(FileExistsError):self.build()
        self.assertEqual(self.archive.read_bytes(),b'existing download')


if __name__=='__main__':unittest.main()
