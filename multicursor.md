How to Use

1. Add cursors: Navigate to desired positions and press Ctrl+N at each location
   - Status message shows cursor count: "3 cursors"
   - Duplicate positions are automatically prevented
2. Enter insert mode: Press i, a, I, A, o, or O as usual
   - Text will be inserted at ALL cursor positions simultaneously
   - All cursors are automatically cleared after the edit
3. Clear cursors: Press Ctrl+\ to remove all cursors without editing
   - Status message confirms: "cursors cleared"

Example Workflow

# Edit multiple similar lines at once:
1. Position cursor at first location, press Ctrl+N
2. Move to second location, press Ctrl+N
3. Move to third location, press Ctrl+N
     → Status shows "3 cursors"
4. Press 'i' to enter insert mode
5. Type your text (appears at all 3 positions)
6. Press Esc to exit insert mode
     → All edits applied, cursors cleared

Technical Details

- Memory efficient: Dynamic array grows as needed
- Undo/redo compatible: Multi-cursor edits are a single step; undo (u) reverts all insertions at once, and redo (Ctrl+R) reapplies them
- UTF-8 aware: Handles multi-byte characters correctly
- No position conflicts: Duplicate cursor prevention built-in
- All insert commands: Works with i, a, I, A, o, O
