 /*
  * This file is part of the KDE project
  * Copyright (C) 2019 Sharaf Zaman <sharafzaz121@gmail.com>
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
  */

package org.krita.android;

 import android.os.Bundle;
 import android.content.Intent;
 import android.net.Uri;
 import android.view.InputDevice;
 import android.view.KeyEvent;
 import android.view.MotionEvent;
 import android.view.WindowManager;

 import org.qtproject.qt5.android.QtNative;
 import org.qtproject.qt5.android.bindings.QtActivity;

public class MainActivity extends QtActivity {

	public boolean isStartup = true;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
		                     WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // we have to do this before loading main()
        Intent i = getIntent();
        String uri = addToKnownUris(i);
        if (uri != null) {
            // this will be passed as a command line argument to main()
            i.putExtra("applicationArguments", uri);
        }

		super.onCreate(savedInstanceState);
		new ConfigsManager().handleAssets(this);

        DonationHelper.getInstance();
	}

    @Override
    protected void onNewIntent (Intent intent) {
        String uri = addToKnownUris(intent);
        if (uri != null) {
            JNIWrappers.openFileFromIntent(uri);
        }

        super.onNewIntent(intent);
    }

    private String addToKnownUris(Intent intent) {
        if (intent != null) {
            Uri fileUri = intent.getData();
            if (fileUri != null) {
                int modeFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
                if ((intent.getFlags() & Intent.FLAG_GRANT_WRITE_URI_PERMISSION) != 0) {
                    modeFlags |= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                }
                QtNative.addToKnownUri(fileUri, modeFlags);
                return fileUri.toString();
            }
        }
        return null;
    }

	@Override
	public void onPause() {
		super.onPause();
		// onPause() _is_ called when the app starts. If the native lib
		// isn't loaded, it crashes.
		if (!isStartup) {
			JNIWrappers.saveState();
		}
		else {
			isStartup = false;
		}
	}

	@Override
	public boolean onKeyUp(final int keyCode, final KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK && getActionBar() != null &&
		    !getActionBar().isShowing()) {
			JNIWrappers.exitFullScreen();
			return true;
		}

		return super.onKeyUp(keyCode, event);
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
        // We manually pass these events to the QPA Android because,
        // android doesn't send events of type other than SOURCE_CLASS_POINTER
        // to the view which was just tapped. So, this view will never get to
        // QtSurface, because it doesn't claim focus.
		if (event.isFromSource(InputDevice.SOURCE_TOUCHPAD)) {
			return QtNative.sendGenericMotionEvent(event, event.getDeviceId());
		}
		return super.onGenericMotionEvent(event);
	}
}
