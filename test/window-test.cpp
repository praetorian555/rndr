#include <algorithm>
#include <limits>
#include <utility>

#include <catch2/catch2.hpp>

#include "rndr/application.hpp"
#include "rndr/generic-window.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

#if RNDR_LINUX
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <poll.h>
#include <xcb/xcb.h>
#endif

using namespace Rndr;

namespace
{
// A window that never shows, so the suite does not steal focus from whoever is using the machine.
Opal::Ref<GenericWindow> CreateHiddenWindow(Application& app)
{
    return app.CreateGenericWindow({.width = 64,
                                    .height = 64,
                                    .name = "Window test",
                                    .resizable = false,
                                    .has_title_bar = false,
                                    .has_border = false,
                                    .show_in_taskbar = false,
                                    .start_visible = false})
        .GetValue();
}
}  // namespace

TEST_CASE("A window keeps the cursor shape it is given", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = CreateHiddenWindow(*app);

    REQUIRE(window->GetCursorShape() == CursorShape::Arrow);

    window->SetCursorShape(CursorShape::IBeam);
    REQUIRE(window->GetCursorShape() == CursorShape::IBeam);

    window->SetCursorShape(CursorShape::ResizeHorizontal);
    REQUIRE(window->GetCursorShape() == CursorShape::ResizeHorizontal);

    // Count is not a shape: the window keeps the one it had.
    window->SetCursorShape(CursorShape::Count);
    REQUIRE(window->GetCursorShape() == CursorShape::ResizeHorizontal);

    app->DestroyGenericWindow(std::move(window));
}

TEST_CASE("A desktop window records the orientation it is asked for and stays as it is", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = app->CreateGenericWindow({.width = 64,
                                                                .height = 48,
                                                                .name = "Window test",
                                                                .resizable = false,
                                                                .has_title_bar = false,
                                                                .has_border = false,
                                                                .show_in_taskbar = false,
                                                                .start_visible = false,
                                                                .orientation = ScreenOrientation::Portrait})
                                          .GetValue();
    const Vector2i size = window->GetSize();

    REQUIRE(window->GetOrientation() == ScreenOrientation::Portrait);

    REQUIRE(window->SetOrientation(ScreenOrientation::Landscape) == ErrorCode::Success);
    REQUIRE(window->GetOrientation() == ScreenOrientation::Landscape);
    app->ProcessSystemEvents();
    REQUIRE(window->GetSize() == size);

    REQUIRE(window->SetOrientation(ScreenOrientation::Any) == ErrorCode::Success);
    REQUIRE(window->GetOrientation() == ScreenOrientation::Any);

    app->DestroyGenericWindow(std::move(window));
}

TEST_CASE("A desktop window records the refresh rate it is asked for", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = app->CreateGenericWindow({.width = 64,
                                                                .height = 48,
                                                                .name = "Window test",
                                                                .resizable = false,
                                                                .has_title_bar = false,
                                                                .has_border = false,
                                                                .show_in_taskbar = false,
                                                                .start_visible = false,
                                                                .preferred_refresh_rate = 120.0f})
                                          .GetValue();
    REQUIRE(window->GetPreferredRefreshRate() == 120.0f);

    REQUIRE(window->SetPreferredRefreshRate(60.0f) == ErrorCode::Success);
    REQUIRE(window->GetPreferredRefreshRate() == 60.0f);
    REQUIRE(window->SetPreferredRefreshRate(0.0f) == ErrorCode::Success);
    REQUIRE(window->GetPreferredRefreshRate() == 0.0f);

    // Refused, and the last good rate kept.
    REQUIRE(window->SetPreferredRefreshRate(-1.0f) == ErrorCode::InvalidArgument);
    REQUIRE(window->SetPreferredRefreshRate(std::numeric_limits<f32>::quiet_NaN()) == ErrorCode::InvalidArgument);
    REQUIRE(window->GetPreferredRefreshRate() == 0.0f);

    app->DestroyGenericWindow(std::move(window));
}

TEST_CASE("A desktop window has no safe insets", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = CreateHiddenWindow(*app);

    // The client area is the application's; nothing the system draws covers it.
    REQUIRE(window->GetSafeInsets() == SafeInsets{});

    app->DestroyGenericWindow(std::move(window));
}

TEST_CASE("A desktop window has no on-screen keyboard over it", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = CreateHiddenWindow(*app);

    // Asking for text records the request; no keyboard comes up over the window.
    REQUIRE(app->StartTextInput() == ErrorCode::Success);
    app->ProcessSystemEvents();
    REQUIRE(window->GetOnScreenKeyboard() == OnScreenKeyboard{});
    REQUIRE(app->StopTextInput() == ErrorCode::Success);

    app->DestroyGenericWindow(std::move(window));
}

#if RNDR_WINDOWS
TEST_CASE("Every cursor shape maps to a system cursor", "[window]")
{
    for (u8 shape = 0; shape <= static_cast<u8>(CursorShape::Count); ++shape)
    {
        INFO("shape " << static_cast<int>(shape));
        REQUIRE(GetSystemCursor(static_cast<CursorShape>(shape)) != nullptr);
    }
    // Count falls back to the arrow.
    REQUIRE(GetSystemCursor(CursorShape::Count) == GetSystemCursor(CursorShape::Arrow));
    REQUIRE(GetSystemCursor(CursorShape::IBeam) != GetSystemCursor(CursorShape::Arrow));
}
#endif

namespace
{
// UTF-8 spelled out, since the sources are not compiled as UTF-8 everywhere: "Copy ✓ 日本 🙂".
constexpr const char* k_clipboard_text = "Copy \xE2\x9C\x93 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x99\x82";
}  // namespace

TEST_CASE("The clipboard gives back the text put on it", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();

    // Put back afterwards, so running the suite does not eat whatever the person at the machine copied. On
    // Linux the clipboard goes with the application that owns it, so there it only lasts until the case ends.
    Opal::Expected<Opal::StringUtf8, ErrorCode> before = app->GetClipboardText();

    SECTION("Text round trips unchanged")
    {
        const Opal::StringUtf8 text(k_clipboard_text);
        REQUIRE(app->SetClipboardText(text) == ErrorCode::Success);
        Opal::Expected<Opal::StringUtf8, ErrorCode> read = app->GetClipboardText();
        REQUIRE(read.HasValue());
        REQUIRE(read.GetValue() == text);
    }
    SECTION("Empty text leaves an empty clipboard")
    {
        REQUIRE(app->SetClipboardText(Opal::StringUtf8(k_clipboard_text)) == ErrorCode::Success);
        REQUIRE(app->SetClipboardText(Opal::StringUtf8()) == ErrorCode::Success);
        Opal::Expected<Opal::StringUtf8, ErrorCode> read = app->GetClipboardText();
        REQUIRE(read.HasValue());
        REQUIRE(read.GetValue().GetSize() == 0);
    }
    SECTION("Text that is not UTF-8 is refused and the clipboard is left alone")
    {
        REQUIRE(app->SetClipboardText(Opal::StringUtf8(k_clipboard_text)) == ErrorCode::Success);
        REQUIRE(app->SetClipboardText(Opal::StringUtf8("\xFF\xFE")) == ErrorCode::InvalidArgument);
        Opal::Expected<Opal::StringUtf8, ErrorCode> read = app->GetClipboardText();
        REQUIRE(read.HasValue());
        REQUIRE(read.GetValue() == Opal::StringUtf8(k_clipboard_text));
    }

    if (before.HasValue() && before.GetValue().GetSize() > 0)
    {
        REQUIRE(app->SetClipboardText(before.GetValue()) == ErrorCode::Success);
    }
}

#if RNDR_LINUX
namespace
{
/**
 * A second X client, standing in for whatever other application the clipboard is shared with: it asks the
 * application under test for the clipboard, or owns it and answers the application the way another program
 * would, in one piece or through INCR.
 */
class X11ClipboardPeer
{
public:
    X11ClipboardPeer()
    {
        m_connection = xcb_connect(nullptr, nullptr);
        m_connection_error = xcb_connection_has_error(m_connection);
        if (m_connection_error != 0)
        {
            xcb_disconnect(m_connection);
            m_connection = nullptr;
            return;
        }
        xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;
        m_window = xcb_generate_id(m_connection);
        const u32 event_mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
        xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, m_window, screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
                          XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &event_mask);
        clipboard = Intern("CLIPBOARD");
        utf8_string = Intern("UTF8_STRING");
        targets = Intern("TARGETS");
        incr = Intern("INCR");
        property = Intern("RNDR_TEST_PASTE");
        application_property = Intern("RNDR_CLIPBOARD");
    }
    ~X11ClipboardPeer()
    {
        if (m_connection != nullptr)
        {
            xcb_destroy_window(m_connection, m_window);
            xcb_disconnect(m_connection);
        }
    }
    X11ClipboardPeer(const X11ClipboardPeer&) = delete;
    X11ClipboardPeer& operator=(const X11ClipboardPeer&) = delete;

    [[nodiscard]] bool IsConnected() const { return m_connection != nullptr; }
    /** What xcb_connection_has_error said about the connection, one of the XCB_CONN_ERROR codes. */
    [[nodiscard]] i32 GetConnectionError() const { return m_connection_error; }
    [[nodiscard]] xcb_connection_t* GetConnection() const { return m_connection; }
    [[nodiscard]] xcb_window_t GetWindow() const { return m_window; }

    /** The next event `is_wanted` accepts, dropping the rest, or null once `timeout_ms` has passed. */
    template <typename Predicate>
    xcb_generic_event_t* WaitFor(Predicate is_wanted, i32 timeout_ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (true)
        {
            xcb_generic_event_t* event = xcb_poll_for_event(m_connection);
            if (event != nullptr)
            {
                if (is_wanted(event))
                {
                    return event;
                }
                free(event);
                continue;
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
            {
                return nullptr;
            }
            pollfd poll_desc = {.fd = xcb_get_file_descriptor(m_connection), .events = POLLIN, .revents = 0};
            poll(&poll_desc, 1, static_cast<int>(remaining.count()));
        }
    }

    /** Reads and deletes the property the application answered into, as the given type. */
    [[nodiscard]] Opal::DynamicArray<u8> TakeProperty(xcb_atom_t& out_type)
    {
        xcb_get_property_reply_t* reply = xcb_get_property_reply(
            m_connection, xcb_get_property(m_connection, 1, m_window, property, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX / 4), nullptr);
        Opal::DynamicArray<u8> bytes;
        out_type = XCB_ATOM_NONE;
        if (reply != nullptr)
        {
            out_type = reply->type;
            const auto* data = static_cast<const u8*>(xcb_get_property_value(reply));
            bytes.Append(Opal::ArrayView<const u8>(data, static_cast<u64>(xcb_get_property_value_length(reply))));
            free(reply);
        }
        return bytes;
    }

    /**
     * Answers requests for the clipboard with `text` until the application's own has been answered, in pieces
     * of at most `piece_size` bytes through INCR when that is set. Others are answered too, whole: a desktop
     * that mirrors the X clipboard - WSLg does - asks every new owner for its text as well. Runs on its own
     * thread while the application waits on the answer, so it reports through its return value rather than
     * through Catch.
     */
    bool ServeApplication(const Opal::StringUtf8& text, u32 piece_size)
    {
        while (true)
        {
            xcb_generic_event_t* event = WaitFor([](const xcb_generic_event_t* candidate)
                                                 { return (candidate->response_type & 0x7F) == XCB_SELECTION_REQUEST; },
                                                 2000);
            if (event == nullptr)
            {
                return false;
            }
            const xcb_selection_request_event_t request = *reinterpret_cast<xcb_selection_request_event_t*>(event);
            free(event);
            if (request.property == application_property)
            {
                return request.target == utf8_string && Answer(request, text, piece_size);
            }
            Answer(request, text, 0);
        }
    }

    xcb_atom_t clipboard = XCB_ATOM_NONE;
    xcb_atom_t utf8_string = XCB_ATOM_NONE;
    xcb_atom_t targets = XCB_ATOM_NONE;
    xcb_atom_t incr = XCB_ATOM_NONE;
    xcb_atom_t property = XCB_ATOM_NONE;
    /** The property the application under test asks for the clipboard to be written into. */
    xcb_atom_t application_property = XCB_ATOM_NONE;

private:
    bool Answer(const xcb_selection_request_event_t& request, const Opal::StringUtf8& text, u32 piece_size)
    {
        xcb_selection_notify_event_t notify = {};
        notify.response_type = XCB_SELECTION_NOTIFY;
        notify.time = request.time;
        notify.requestor = request.requestor;
        notify.selection = request.selection;
        notify.target = request.target;
        notify.property = XCB_ATOM_NONE;
        if (request.target == targets)
        {
            const xcb_atom_t offered[] = {targets, utf8_string};
            xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, request.requestor, request.property, XCB_ATOM_ATOM, 32, 2, offered);
            notify.property = request.property;
        }
        if (request.target != utf8_string)
        {
            xcb_send_event(m_connection, 0, request.requestor, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char*>(&notify));
            xcb_flush(m_connection);
            return false;
        }

        const bool use_incr = piece_size > 0 && text.GetSize() > piece_size;
        if (use_incr)
        {
            // Watch the requestor's property so each delete, which asks for the next piece, is seen.
            const u32 event_mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
            xcb_change_window_attributes(m_connection, request.requestor, XCB_CW_EVENT_MASK, &event_mask);
            const u32 size = static_cast<u32>(text.GetSize());
            xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, request.requestor, request.property, incr, 32, 1, &size);
        }
        else
        {
            xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, request.requestor, request.property, utf8_string, 8,
                                static_cast<u32>(text.GetSize()), text.GetData());
        }
        notify.property = request.property;
        xcb_send_event(m_connection, 0, request.requestor, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char*>(&notify));
        xcb_flush(m_connection);
        if (!use_incr)
        {
            return true;
        }

        u64 offset = 0;
        while (true)
        {
            xcb_generic_event_t* deleted = WaitFor(
                [&](const xcb_generic_event_t* candidate)
                {
                    if ((candidate->response_type & 0x7F) != XCB_PROPERTY_NOTIFY)
                    {
                        return false;
                    }
                    const auto* notify_event = reinterpret_cast<const xcb_property_notify_event_t*>(candidate);
                    return notify_event->window == request.requestor && notify_event->atom == request.property &&
                           notify_event->state == XCB_PROPERTY_DELETE;
                },
                2000);
            if (deleted == nullptr)
            {
                return false;
            }
            free(deleted);
            // The last piece is empty, which is how the requestor knows it has everything.
            const u64 piece = std::min<u64>(piece_size, text.GetSize() - offset);
            xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, request.requestor, request.property, utf8_string, 8,
                                static_cast<u32>(piece), text.GetData() + offset);
            xcb_flush(m_connection);
            if (piece == 0)
            {
                return true;
            }
            offset += piece;
        }
    }

    xcb_atom_t Intern(const char* name)
    {
        xcb_intern_atom_reply_t* reply =
            xcb_intern_atom_reply(m_connection, xcb_intern_atom(m_connection, 0, static_cast<u16>(strlen(name)), name), nullptr);
        xcb_atom_t atom = XCB_ATOM_NONE;
        if (reply != nullptr)
        {
            atom = reply->atom;
        }
        free(reply);
        return atom;
    }

    xcb_connection_t* m_connection = nullptr;
    i32 m_connection_error = 0;
    xcb_window_t m_window = XCB_NONE;
};

/** Asks for the clipboard as `target` and pumps the application until its answer reaches the peer. */
xcb_atom_t RequestFromApplication(Application& app, X11ClipboardPeer& peer, xcb_atom_t target)
{
    xcb_convert_selection(peer.GetConnection(), peer.GetWindow(), peer.clipboard, target, peer.property, XCB_CURRENT_TIME);
    xcb_flush(peer.GetConnection());
    for (i32 attempt = 0; attempt < 200; ++attempt)
    {
        // The application answers from its event pump, so it has to run for the answer to go out.
        app.ProcessSystemEvents(10);
        xcb_generic_event_t* event = peer.WaitFor([](const xcb_generic_event_t* candidate)
                                                  { return (candidate->response_type & 0x7F) == XCB_SELECTION_NOTIFY; },
                                                  0);
        if (event != nullptr)
        {
            const xcb_atom_t property = reinterpret_cast<xcb_selection_notify_event_t*>(event)->property;
            free(event);
            return property;
        }
    }
    return XCB_ATOM_NONE;
}
}  // namespace

TEST_CASE("Another X client pastes what the application copied", "[window]")
{
    // The application connects first. An X server with no clients left - the previous case just closed its
    // application - resets itself, and a connection made in the middle of that fails.
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    X11ClipboardPeer peer;
    INFO("xcb connection error " << peer.GetConnectionError());
    REQUIRE(peer.IsConnected());
    const Opal::StringUtf8 text(k_clipboard_text);
    REQUIRE(app->SetClipboardText(text) == ErrorCode::Success);

    SECTION("As UTF-8 text")
    {
        REQUIRE(RequestFromApplication(*app, peer, peer.utf8_string) == peer.property);
        xcb_atom_t type = XCB_ATOM_NONE;
        const Opal::DynamicArray<u8> bytes = peer.TakeProperty(type);
        REQUIRE(type == peer.utf8_string);
        REQUIRE(Opal::StringUtf8(reinterpret_cast<const char8*>(bytes.GetData()), bytes.GetSize()) == text);
    }
    SECTION("The targets it offers include UTF-8 text")
    {
        REQUIRE(RequestFromApplication(*app, peer, peer.targets) == peer.property);
        xcb_atom_t type = XCB_ATOM_NONE;
        const Opal::DynamicArray<u8> bytes = peer.TakeProperty(type);
        REQUIRE(type == XCB_ATOM_ATOM);
        const auto* atoms = reinterpret_cast<const xcb_atom_t*>(bytes.GetData());
        const u64 atom_count = bytes.GetSize() / sizeof(xcb_atom_t);
        REQUIRE(std::find(atoms, atoms + atom_count, peer.utf8_string) != atoms + atom_count);
    }
    SECTION("A target it cannot give is refused")
    {
        REQUIRE(RequestFromApplication(*app, peer, XCB_ATOM_PIXMAP) == XCB_ATOM_NONE);
    }
}

TEST_CASE("The application pastes what another X client copied", "[window]")
{
    // The application connects first, for the reason the case above gives.
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    X11ClipboardPeer peer;
    INFO("xcb connection error " << peer.GetConnectionError());
    REQUIRE(peer.IsConnected());
    xcb_set_selection_owner(peer.GetConnection(), peer.GetWindow(), peer.clipboard, XCB_CURRENT_TIME);
    xcb_flush(peer.GetConnection());

    /** GetClipboardText blocks until the owner answers, so the peer answers from a thread of its own. */
    auto paste_served_by_peer = [&](const Opal::StringUtf8& text, u32 piece_size)
    {
        bool served = false;
        std::thread owner([&] { served = peer.ServeApplication(text, piece_size); });
        Opal::Expected<Opal::StringUtf8, ErrorCode> read = app->GetClipboardText();
        owner.join();
        return std::make_pair(served, std::move(read));
    };

    SECTION("In one piece")
    {
        const Opal::StringUtf8 text(k_clipboard_text);
        auto [served, read] = paste_served_by_peer(text, 0);
        REQUIRE(served);
        REQUIRE(read.HasValue());
        REQUIRE(read.GetValue() == text);
    }
    SECTION("In pieces, through INCR")
    {
        // Large enough for several pieces, and not a multiple of the piece size so the last one is short.
        Opal::StringUtf8 text;
        for (i32 i = 0; i < 1000; ++i)
        {
            text.Append(Opal::StringUtf8(k_clipboard_text));
        }
        auto [served, read] = paste_served_by_peer(text, 4096);
        REQUIRE(served);
        REQUIRE(read.HasValue());
        REQUIRE(read.GetValue().GetSize() == text.GetSize());
        REQUIRE(read.GetValue() == text);
    }
}
#endif
