package com.idas3.unity;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.os.Bundle;

import com.unity3d.player.UnityPlayer;
import com.unity3d.player.UnityPlayerActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public final class Idas3Activity extends UnityPlayerActivity {
    private static final int ROM_REQUEST = 193031;
    private static String callbackObject;

    public static void openRomPicker(String objectName) {
        Activity activity = UnityPlayer.currentActivity;
        if (activity == null) return;
        callbackObject = objectName;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        activity.startActivityForResult(intent, ROM_REQUEST);
    }

    public static boolean copyUriToFile(String uriText, String destination) {
        Activity activity = UnityPlayer.currentActivity;
        if (activity == null || uriText == null || destination == null) return false;
        File output = new File(destination);
        File parent = output.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) return false;
        try (InputStream input = activity.getContentResolver().openInputStream(Uri.parse(uriText));
             OutputStream outputStream = new FileOutputStream(output, false)) {
            if (input == null) return false;
            byte[] buffer = new byte[1024 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                if (count != 0) outputStream.write(buffer, 0, count);
            }
            outputStream.flush();
            return true;
        } catch (Exception error) {
            if (output.exists()) output.delete();
            return false;
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != ROM_REQUEST || callbackObject == null) return;
        ArrayList<String> entries = new ArrayList<>();
        if (resultCode == RESULT_OK && data != null) {
            if (data.getClipData() != null) {
                for (int i = 0; i < data.getClipData().getItemCount(); ++i)
                    addEntry(data.getClipData().getItemAt(i).getUri(), entries);
            } else if (data.getData() != null) addEntry(data.getData(), entries);
        }
        StringBuilder result = new StringBuilder();
        for (String entry : entries) {
            if (result.length() != 0) result.append('\n');
            result.append(entry);
        }
        UnityPlayer.UnitySendMessage(callbackObject, "OnAndroidRomUris", result.toString());
        callbackObject = null;
    }

    private void addEntry(Uri uri, ArrayList<String> entries) {
        if (uri == null) return;
        String name = uri.getLastPathSegment();
        try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
        } catch (Exception ignored) { }
        if (name == null) name = "";
        name = name.replace('\n', '_').replace('\r', '_').replace('\t', '_');
        entries.add(uri.toString() + '\t' + name);
    }
}
