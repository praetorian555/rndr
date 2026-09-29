package dev.rndr;

import android.Manifest;
import android.app.NativeActivity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

/**
 * The NativeActivity rndr's Android platform layer runs in, plus the one thing NativeActivity cannot do: take text from
 * the on-screen keyboard. A keyboard hands text to the focused view's InputConnection, and NativeActivity's own view
 * has none, so an app on it gets at most the key events a keyboard falls back to - no autocorrect, no swipe typing,
 * nothing outside Latin. This adds an invisible view that has one and passes what is typed into it to native code.
 *
 * Everything else is NativeActivity's: the window, the input queue, android_main. An application declares this class
 * as its activity in the manifest instead of android.app.NativeActivity; one that does not keeps working, and gets
 * key events only.
 */
public class RndrActivity extends NativeActivity {
    private static final String TAG = "Rndr";
    private static final int LOCATION_PERMISSION_REQUEST = 0x4c4f;

    private TextInputView textInputView;
    /** The GPS listener while location updates run, touched only on the UI thread. */
    private LocationListener locationListener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        textInputView = new TextInputView(this);
        addContentView(textInputView, new ViewGroup.LayoutParams(1, 1));
        // A turn from one landscape to the other changes neither the size nor the configuration, so native code hears
        // of nothing; the insets swap sides all the same, and this is where that shows. So does the on-screen keyboard,
        // which changes no size either: the window stays the whole screen, and the keyboard is an inset over it.
        getWindow().getDecorView().setOnApplyWindowInsetsListener((view, insets) -> {
            notifyWindowInsetsChanged(insets);
            return view.onApplyWindowInsets(insets);
        });
    }

    private static void notifyWindowInsetsChanged(WindowInsets insets) {
        boolean keyboardVisible = false;
        int keyboardHeight = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            keyboardVisible = insets.isVisible(WindowInsets.Type.ime());
            keyboardHeight = insets.getInsets(WindowInsets.Type.ime()).bottom;
        }
        try {
            nativeWindowInsetsChanged(keyboardVisible, keyboardHeight);
        } catch (UnsatisfiedLinkError e) {
            // The first insets can arrive before android_main has registered the method. The window reads its insets
            // when it is created, and the keyboard is down until text input starts, so there is nothing to catch up on.
        }
    }

    /**
     * Show the on-screen keyboard and point it at the text input view, or hide it. Called by native code on its own
     * thread; a view is only touched on the UI thread, so the work is posted there.
     */
    public void setTextInputActive(final boolean active) {
        runOnUiThread(() -> {
            InputMethodManager inputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            if (active) {
                textInputView.setFocusable(true);
                textInputView.setFocusableInTouchMode(true);
                textInputView.requestFocus();
                inputMethodManager.restartInput(textInputView);
                inputMethodManager.showSoftInput(textInputView, 0);
            } else {
                inputMethodManager.hideSoftInputFromWindow(textInputView.getWindowToken(), 0);
                textInputView.clearFocus();
                textInputView.setFocusable(false);
                textInputView.setFocusableInTouchMode(false);
            }
        });
    }

    /**
     * Dark icons in the status and navigation bars, for a light window background, or light ones for a dark background.
     * Called by native code on its own thread; the window is only touched on the UI thread, so the work is posted there.
     */
    @SuppressWarnings("deprecation")
    public void setSystemBarsDarkContent(final boolean dark) {
        runOnUiThread(() -> {
            View decor = getWindow().getDecorView();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsetsController controller = decor.getWindowInsetsController();
                if (controller == null) {
                    return;
                }
                int mask = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
                controller.setSystemBarsAppearance(dark ? mask : 0, mask);
            } else {
                int bits = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                int flags = decor.getSystemUiVisibility();
                decor.setSystemUiVisibility(dark ? (flags | bits) : (flags & ~bits));
            }
        });
    }

    /**
     * Buzz for a moment at the default strength. Called by native code on its own thread. Does nothing on a device without
     * a vibrator, and nothing when the manifest does not ask for android.permission.VIBRATE.
     *
     * @return Whether the vibrator was asked to run.
     */
    @SuppressWarnings("deprecation")
    public boolean vibrate(final int milliseconds) {
        Vibrator vibrator;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager manager = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            vibrator = manager != null ? manager.getDefaultVibrator() : null;
        } else {
            vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        }
        if (vibrator == null || !vibrator.hasVibrator()) {
            return false;
        }
        try {
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
            return true;
        } catch (SecurityException e) {
            return false;
        }
    }

    /** Whether the precise location may be read. Called by native code on its own thread. */
    public boolean hasLocationPermission() {
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Ask for the precise location, and on Android 13 and later for posting notifications, which the tracking
     * notification needs to be seen. The answer goes to nativeLocationPermissionChanged; when everything was already
     * granted the system answers at once, without a prompt. Called by native code on its own thread.
     */
    public void requestLocationPermission() {
        runOnUiThread(() -> {
            String[] permissions;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                permissions = new String[] {Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION,
                                            Manifest.permission.POST_NOTIFICATIONS};
            } else {
                permissions = new String[] {Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION};
            }
            requestPermissions(permissions, LOCATION_PERMISSION_REQUEST);
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != LOCATION_PERMISSION_REQUEST) {
            return;
        }
        try {
            nativeLocationPermissionChanged(hasLocationPermission());
        } catch (UnsatisfiedLinkError e) {
            // android_main is gone; nobody is waiting for the answer.
        }
    }

    /**
     * Start GPS updates, replacing any that run, and deliver each fix to nativeLocationFix. With keepRunningInBackground
     * a foreground service of type location runs alongside, showing the title and text, so that the updates keep coming
     * with the screen off; the manifest must declare RndrActivity$LocationService for it. Called by native code on its
     * own thread; the listener is registered on the UI thread.
     *
     * @return 0 when started, 1 without the permission, 2 when location is switched off in the settings, 3 without a GPS.
     */
    public int startLocationUpdates(final int intervalMs, final boolean keepRunningInBackground, final String title, final String text) {
        if (!hasLocationPermission()) {
            return 1;
        }
        final LocationManager manager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        if (manager == null || !manager.getAllProviders().contains(LocationManager.GPS_PROVIDER)) {
            return 3;
        }
        if (!manager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            return 2;
        }
        runOnUiThread(() -> {
            removeLocationListener(manager);
            locationListener = new FixListener();
            try {
                manager.requestLocationUpdates(LocationManager.GPS_PROVIDER, intervalMs, 0.0f, locationListener, Looper.getMainLooper());
            } catch (SecurityException e) {
                Log.w(TAG, "The location permission was taken away before the updates could start", e);
                locationListener = null;
                return;
            }
            Intent service = new Intent(this, LocationService.class);
            if (!keepRunningInBackground) {
                stopService(service);
                return;
            }
            service.putExtra(LocationService.EXTRA_TITLE, title);
            service.putExtra(LocationService.EXTRA_TEXT, text);
            try {
                startForegroundService(service);
            } catch (RuntimeException e) {
                // Android 12 refuses a foreground service started from the background. The updates still run while
                // the activity is in use.
                Log.w(TAG, "The location service could not start; tracking stops when the app goes to the background", e);
            }
        });
        return 0;
    }

    /** Stop the GPS updates and the service startLocationUpdates started. Called by native code on its own thread. */
    public void stopLocationUpdates() {
        runOnUiThread(() -> {
            removeLocationListener((LocationManager) getSystemService(Context.LOCATION_SERVICE));
            stopService(new Intent(this, LocationService.class));
        });
    }

    private void removeLocationListener(LocationManager manager) {
        if (locationListener != null && manager != null) {
            manager.removeUpdates(locationListener);
        }
        locationListener = null;
    }

    @Override
    protected void onDestroy() {
        removeLocationListener((LocationManager) getSystemService(Context.LOCATION_SERVICE));
        stopService(new Intent(this, LocationService.class));
        super.onDestroy();
    }

    /** Hands each fix to native code. Runs on the UI thread, whose looper the updates were requested on. */
    private static final class FixListener implements LocationListener {
        @Override
        public void onLocationChanged(Location location) {
            try {
                nativeLocationFix(location.getLatitude(), location.getLongitude(), location.hasAccuracy() ? location.getAccuracy() : -1.0f,
                                  location.hasSpeed() ? location.getSpeed() : -1.0f, location.getElapsedRealtimeNanos());
            } catch (UnsatisfiedLinkError e) {
                // android_main is gone.
            }
        }

        // Abstract below API 30.
        @Override
        public void onProviderEnabled(String provider) {}

        @Override
        public void onProviderDisabled(String provider) {}

        @Override
        @SuppressWarnings("deprecation")
        public void onStatusChanged(String provider, int status, Bundle extras) {}
    }

    /**
     * The foreground service that keeps location updates coming while the activity is in the background or the screen
     * is off. It holds nothing but its notification: the updates are the activity's, and a running foreground service of
     * type location is what lets the app keep receiving them. Declared by the application's manifest as
     * dev.rndr.RndrActivity$LocationService with android:foregroundServiceType="location".
     */
    public static class LocationService extends Service {
        static final String EXTRA_TITLE = "dev.rndr.location.title";
        static final String EXTRA_TEXT = "dev.rndr.location.text";
        private static final String CHANNEL_ID = "dev.rndr.location";
        private static final int NOTIFICATION_ID = 0x4c4f43;

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            String title = intent != null ? intent.getStringExtra(EXTRA_TITLE) : null;
            String text = intent != null ? intent.getStringExtra(EXTRA_TEXT) : null;
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(new NotificationChannel(CHANNEL_ID, "Location tracking", NotificationManager.IMPORTANCE_LOW));
            }
            Notification.Builder builder = new Notification.Builder(this, CHANNEL_ID)
                                               .setContentTitle(title)
                                               .setContentText(text)
                                               .setSmallIcon(android.R.drawable.ic_menu_mylocation)
                                               .setOngoing(true);
            // The launcher's intent brings the running task forward rather than starting a second activity.
            Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
            if (launch != null) {
                launch.addFlags(Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                builder.setContentIntent(PendingIntent.getActivity(this, 0, launch, PendingIntent.FLAG_IMMUTABLE));
            }
            try {
                startForeground(NOTIFICATION_ID, builder.build(), ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION);
            } catch (RuntimeException e) {
                // Android 14 refuses a location service without the location permission, or without
                // FOREGROUND_SERVICE_LOCATION in the manifest.
                Log.w(TAG, "The location service could not come to the foreground", e);
                stopSelf();
            }
            return START_NOT_STICKY;
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }
    }

    /** Text the keyboard committed. Registered by AndroidApplication; runs on the UI thread. */
    static native void nativeCommitText(String text);

    /**
     * The window's insets changed. Carries the on-screen keyboard's, which native code cannot ask for itself: whether
     * it is up, and how far it covers the window from the bottom edge. Registered by AndroidApplication; runs on the UI
     * thread.
     */
    static native void nativeWindowInsetsChanged(boolean keyboardVisible, int keyboardHeight);

    /**
     * A GPS fix: degrees, the accuracy radius and the speed (negative when unknown), and when it was taken on the
     * elapsed-realtime clock. Registered by AndroidApplication; runs on the UI thread.
     */
    static native void nativeLocationFix(double latitude, double longitude, float accuracy, float speed, long elapsedRealtimeNanos);

    /** The answer to requestLocationPermission. Registered by AndroidApplication; runs on the UI thread. */
    static native void nativeLocationPermissionChanged(boolean granted);

    /**
     * A view with nothing to draw, there to own the InputConnection. Focusable only while text input is active: a
     * window gives its first focusable view the focus when it opens, and some keyboards (Samsung's) show themselves
     * for a focused text editor, so a focusable one from the start put the keyboard up on launch.
     */
    private static final class TextInputView extends View {
        TextInputView(Context context) {
            super(context);
            setFocusable(false);
            setFocusableInTouchMode(false);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo info) {
            // A visible password asks the keyboard for no suggestions, so it commits as it goes instead of holding a
            // word back to correct; the flags keep it from covering the app with a full screen editor in landscape.
            info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
            info.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN;
            return new TextInputConnection(this);
        }
    }

    /**
     * Passes committed text on and keeps none of it, so the editor always looks empty to the keyboard. Backspace
     * therefore arrives as a KEYCODE_DEL key event, the way a hardware key would, and so does Enter.
     */
    private static final class TextInputConnection extends BaseInputConnection {
        /** Text a keyboard is still composing - one that suggests anyway holds a word here until it is done. */
        private CharSequence composing = "";

        TextInputConnection(View view) {
            super(view, true);
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            composing = "";
            if (text.length() > 0) {
                nativeCommitText(text.toString());
            }
            return true;
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            composing = text;
            return true;
        }

        @Override
        public boolean finishComposingText() {
            if (composing.length() > 0) {
                nativeCommitText(composing.toString());
                composing = "";
            }
            return true;
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            for (int i = 0; i < beforeLength; i++) {
                sendKey(KeyEvent.KEYCODE_DEL);
            }
            return true;
        }

        @Override
        public boolean performEditorAction(int action) {
            sendKey(KeyEvent.KEYCODE_ENTER);
            return true;
        }

        private void sendKey(int keyCode) {
            sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
            sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
        }
    }
}
