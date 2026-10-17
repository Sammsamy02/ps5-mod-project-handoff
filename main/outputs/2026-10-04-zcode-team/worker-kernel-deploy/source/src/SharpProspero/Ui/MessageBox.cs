// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Ui;

/// <summary>
/// Builds the two panels an application asks for again and again — a message to acknowledge and a
/// question to answer — and opens them over a <see cref="ModalHost"/>. Each button closes the panel and
/// then runs what you passed, so a confirmation before something is deleted takes one call instead of a
/// hand-assembled panel of a title, a message and buttons.
/// </summary>
/// <remarks>
/// The panel takes the controller until a button is pressed, since the host dims and disables the content
/// behind it. To let cancel dismiss it as well, point the screen's cancel at the host, for example
/// <c>screen.Cancelled = () =&gt; host.Close();</c>.
/// </remarks>
public static class MessageBox
{
    /// <summary>
    /// Opens a panel over <paramref name="host"/> showing <paramref name="title"/> and
    /// <paramref name="message"/> with a single button that closes it and then calls
    /// <paramref name="closed"/>.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="host"/> is null.</exception>
    public static void Alert(ModalHost host, string title, string message, string ok = "OK", Action? closed = null)
    {
        ArgumentNullException.ThrowIfNull(host);
        var panel = new StackPanel()
            .Add(new Label(title ?? ""))
            .Add(new TextBlock(message ?? ""))
            .Add(new Button(ok, () =>
            {
                host.Close();
                closed?.Invoke();
            }));
        host.Show(panel);
    }

    /// <summary>
    /// Opens a panel over <paramref name="host"/> showing <paramref name="title"/> and
    /// <paramref name="message"/> with two buttons. The first closes it and calls
    /// <paramref name="onConfirm"/>; the second closes it and calls <paramref name="onCancel"/>.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="host"/> or <paramref name="onConfirm"/> is null.</exception>
    public static void Confirm(
        ModalHost host,
        string title,
        string message,
        Action onConfirm,
        Action? onCancel = null,
        string confirm = "Yes",
        string cancel = "No")
    {
        ArgumentNullException.ThrowIfNull(host);
        ArgumentNullException.ThrowIfNull(onConfirm);
        var buttons = new Row()
            .Add(new Button(confirm, () =>
            {
                host.Close();
                onConfirm();
            }))
            .Add(new Button(cancel, () =>
            {
                host.Close();
                onCancel?.Invoke();
            }));
        var panel = new StackPanel()
            .Add(new Label(title ?? ""))
            .Add(new TextBlock(message ?? ""))
            .Add(buttons);
        host.Show(panel);
    }
}
