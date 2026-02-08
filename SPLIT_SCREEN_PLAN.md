# Split Screen Editor Plan

## Goal
Display two buffers simultaneously - top half and bottom half of screen. Primary use case: view build output while editing source code.

## Current Architecture

```
+----------------------------------+
|  EditView (full screen)          |
|    - owns screen region          |
|    - has one CmEditBuffer*       |
|    - handles input when active   |
+----------------------------------+
|  Status Line                     |
+----------------------------------+
|  Command Line                    |
+----------------------------------+
```

## Proposed Architecture

```
+----------------------------------+
|  EditView (top)                  |
|    - screen rows 0 to splitRow-1 |
|    - buffer: e.g., source.cpp    |
+----------------------------------+
|  Divider Line (1 row)            |
+----------------------------------+
|  EditView (bottom)               |
|    - screen rows splitRow+1 to N |
|    - buffer: e.g., *build*       |
+----------------------------------+
|  Status Line                     |
+----------------------------------+
|  Command Line                    |
+----------------------------------+
```

---

## Phase 1: EditView Region Parameterization

EditView currently calculates its screen region in `recalcScreenPlacements()`. Need to make this configurable.

### Changes to EditView

```cpp
// New members
int _regionStartRow;    // first screen row for this view
int _regionEndRow;      // last screen row for this view (exclusive of status)

// New methods
void setRegion(int startRow, int endRow);
void recalcScreenPlacements();  // already exists, modify to use region
```

### Modify recalcScreenPlacements()

Currently calculates based on full screen. Change to use `_regionStartRow` and `_regionEndRow` instead of assuming row 0 and `screen->rows()`.

**Verify:** Single view still works after this change.

---

## Phase 2: ScreenEditor Split Management

### New Members in ScreenEditor

```cpp
EditView *editView;         // existing - becomes "top" or "only" view
EditView *editViewBottom;   // new - bottom view when split
int _splitMode;             // 0 = single, 1 = horizontal split
int _splitRow;              // row where divider sits
int _activeView;            // 0 = top, 1 = bottom
```

### New Methods

```cpp
void splitHorizontal();     // split screen in half
void unsplit();             // return to single view
void switchActiveView();    // toggle between top/bottom
void drawDivider();         // draw the split line
EditView* activeEditView(); // return currently active view
```

### Input Routing

All keyboard input goes to `activeEditView()`. Change references from `editView` to `activeEditView()` in input handling code.

---

## Phase 3: Commands

### ESC Commands

| Command | Description |
|---------|-------------|
| `split` | Split screen horizontally, bottom shows *build* or current buffer |
| `unsplit` | Return to single full-screen view |

### Control Key

| Key | Action |
|-----|--------|
| `Ctrl-O` | Switch focus between top and bottom view |

---

## Phase 4: Visual Feedback

### Active View Indicator

- Different status line color or marker for active view
- Or: highlight divider line on active side
- Simple approach: show `[*]` in status line of active view

### Divider Line

- Draw with box-drawing characters or simple dashes
- Could show buffer name of bottom view
- Example: `──── *build* ────────────────────────`

---

## Implementation Order

1. **EditView region parameterization** - make EditView work with partial screen
2. **Test single view** - ensure no regression
3. **Add splitMode tracking** - _splitMode, _splitRow, _activeView
4. **Create second EditView** - instantiate editViewBottom when splitting
5. **Draw divider** - simple horizontal line
6. **Route input** - use activeEditView() for all input
7. **Add commands** - split, unsplit, Ctrl-O
8. **Polish** - visual indicators, edge cases

---

## Edge Cases

- Resize terminal while split - recalc both regions
- Quit while split - should work normally
- Buffer operations (save, load) - operate on active view's buffer
- make command while split - output goes to *build*, could auto-show in bottom

---

## Future Enhancements

- Vertical split (side by side)
- More than 2 splits
- Resize split position with keyboard
- Remember split state across sessions
