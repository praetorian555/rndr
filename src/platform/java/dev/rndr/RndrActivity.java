package dev.rndr;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
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
    private TextInputView textInputView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        textInputView = new TextInputView(this);
        addContentView(textInputView, new ViewGroup.LayoutParams(1, 1));
        // A turn from one landscape to the other changes neither the size nor the configuration, so native code hears
        // of nothing; the insets swap sides all the same, and this is where that shows.
        getWindow().getDecorView().setOnApplyWindowInsetsListener((view, insets) -> {
            notifyWindowInsetsChanged();
            return view.onApplyWindowInsets(insets);
        });
    }

    private static void notifyWindowInsetsChanged() {
        try {
            nativeWindowInsetsChanged();
        } catch (UnsatisfiedLinkError e) {
            // The first insets can arrive before android_main has registered the method. The window reads its insets
            // when it is created, so there is nothing to catch up on.
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

    /** Text the keyboard committed. Registered by AndroidApplication; runs on the UI thread. */
    static native void nativeCommitText(String text);

    /** The window's insets changed. Registered by AndroidApplication; runs on the UI thread. */
    static native void nativeWindowInsetsChanged();

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
