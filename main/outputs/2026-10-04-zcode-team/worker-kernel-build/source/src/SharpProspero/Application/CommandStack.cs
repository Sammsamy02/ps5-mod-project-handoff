// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Application;

/// <summary>A reversible action: something that can be done and then undone.</summary>
public interface ICommand
{
    /// <summary>Performs the action.</summary>
    void Do();

    /// <summary>Reverses the action, restoring the state before <see cref="Do"/>.</summary>
    void Undo();
}

/// <summary>
/// A command that can absorb a following command of the same kind, so a run of small changes — typing
/// characters, dragging a slider — collapses into one undo step instead of many.
/// </summary>
public interface ICoalescingCommand : ICommand
{
    /// <summary>
    /// Tries to merge <paramref name="next"/> into this command. Return true if it was absorbed (so
    /// <paramref name="next"/> is not pushed as its own step); false to keep them separate.
    /// </summary>
    bool TryCoalesceWith(ICommand next);
}

/// <summary>An <see cref="ICommand"/> built from a do and an undo delegate.</summary>
public sealed class DelegateCommand(Action doAction, Action undoAction) : ICommand
{
    private readonly Action _doAction = doAction ?? throw new ArgumentNullException(nameof(doAction));
    private readonly Action _undoAction = undoAction ?? throw new ArgumentNullException(nameof(undoAction));

    /// <inheritdoc/>
    public void Do() => _doAction();

    /// <inheritdoc/>
    public void Undo() => _undoAction();
}

/// <summary>
/// An undo and redo history. Run a change through <see cref="Execute(ICommand)"/> and it is performed and
/// remembered; <see cref="Undo"/> and <see cref="Redo"/> walk the history. Executing a new change after an
/// undo discards the redo branch, as an editor does.
/// </summary>
public sealed class CommandStack
{
    private readonly List<ICommand> _undo = [];
    private readonly List<ICommand> _redo = [];
    private int _limit = int.MaxValue;

    /// <summary>
    /// The most undo steps to keep. When more are executed the oldest are dropped (and can no longer be
    /// undone). Defaults to no limit.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">Set to a value that is not positive.</exception>
    public int Limit
    {
        get => _limit;
        set
        {
            ArgumentOutOfRangeException.ThrowIfNegativeOrZero(value);
            _limit = value;
            TrimToLimit();
        }
    }

    /// <summary>Whether there is a step to undo.</summary>
    public bool CanUndo => _undo.Count > 0;

    /// <summary>Whether there is a step to redo.</summary>
    public bool CanRedo => _redo.Count > 0;

    /// <summary>How many steps can be undone.</summary>
    public int UndoCount => _undo.Count;

    /// <summary>How many steps can be redone.</summary>
    public int RedoCount => _redo.Count;

    /// <summary>Performs <paramref name="command"/>, records it for undo, and clears the redo branch.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="command"/> is null.</exception>
    public void Execute(ICommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        command.Do();
        _redo.Clear();

        // Fold into the previous step when it opts in, so a run of small edits is one undo.
        if (_undo.Count > 0 && _undo[^1] is ICoalescingCommand coalescing && coalescing.TryCoalesceWith(command))
            return;

        _undo.Add(command);
        TrimToLimit();
    }

    /// <summary>Performs a change built from a do and an undo delegate.</summary>
    /// <exception cref="ArgumentNullException">An action is null.</exception>
    public void Execute(Action doAction, Action undoAction) => Execute(new DelegateCommand(doAction, undoAction));

    /// <summary>Undoes the most recent step, moving it to the redo branch. Returns false when there is nothing to undo.</summary>
    public bool Undo()
    {
        if (_undo.Count == 0)
            return false;

        // Run the reversal before moving the command between stacks, so a throwing Undo leaves the history
        // intact rather than losing the step.
        ICommand command = _undo[^1];
        command.Undo();
        _undo.RemoveAt(_undo.Count - 1);
        _redo.Add(command);
        return true;
    }

    /// <summary>Redoes the most recently undone step, moving it back to the undo branch. Returns false when there is nothing to redo.</summary>
    public bool Redo()
    {
        if (_redo.Count == 0)
            return false;

        ICommand command = _redo[^1];
        command.Do();
        _redo.RemoveAt(_redo.Count - 1);
        _undo.Add(command);
        TrimToLimit(); // a lowered Limit must apply here too, not only on Execute
        return true;
    }

    /// <summary>Forgets all history, leaving nothing to undo or redo.</summary>
    public void Clear()
    {
        _undo.Clear();
        _redo.Clear();
    }

    private void TrimToLimit()
    {
        int excess = _undo.Count - _limit;
        if (excess > 0)
            _undo.RemoveRange(0, excess); // drop the oldest steps past the limit
    }
}
