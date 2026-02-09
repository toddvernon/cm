# Multi-Subproject Build System Implementation Plan

## Overview

Extend the project system to support multiple subprojects, each with its own directory, makefile, and file list. ProjectView shows collapsible subproject groups with quick build commands. Build commands target specific subprojects or build all in order.

---

## 1. Project File Format

**New format:**
```json
{
  "projectName": "Cm Development",
  "baseDirectory": "..",
  "subprojects": [
    {
      "name": "cx",
      "directory": "cx",
      "makefile": "makefile",
      "files": [
        "base/string.cpp",
        "buildoutput/buildoutput.cpp"
      ]
    },
    {
      "name": "cx_tests",
      "directory": "cx_tests",
      "makefile": "makefile",
      "files": [
        "cxbuildoutput/cxbuildoutput_test.cpp"
      ]
    },
    {
      "name": "cm",
      "directory": "cx_apps/cm",
      "makefile": "makefile",
      "default": true,
      "files": [
        "Cm.cpp",
        "ScreenEditor.cpp",
        "BuildView.cpp"
      ]
    }
  ]
}
```

**Path Resolution:**
- `baseDirectory` is relative to project file location (or absolute)
- Each subproject's `directory` is relative to `baseDirectory`
- Each file path is relative to its subproject's `directory`

**Example resolution (project file in `cx_apps/cm`):**
- Base: `cx_apps/cm/../` → `/Users/toddvernon/Dropbox/dev/cx`
- cm files: `base + cx_apps/cm + Cm.cpp`
- cx files: `base + cx + base/string.cpp`

**Legacy fallback:** If `subprojects` missing, create implicit single subproject from `projectMakefile` and `files`.

---

## 2. Data Structures

### Project.h

```cpp
struct ProjectSubproject {
    CxString name;
    CxString directory;      // relative to baseDirectory
    CxString makefile;
    int isDefault;
    int isExpanded;          // UI state for ProjectView
    CxSList<CxString*> files; // relative to subproject directory
};

class Project {
  public:
    // ... existing methods ...

    // Subproject access
    int subprojectCount(void);
    ProjectSubproject* subprojectAt(int index);
    ProjectSubproject* findSubproject(CxString name);
    ProjectSubproject* getDefaultSubproject(void);

    // Path resolution
    CxString resolveFilePath(ProjectSubproject *sub, CxString filename);
    CxString resolveFilePath(CxString subprojectName, CxString filename);
    CxString getMakeDirectory(ProjectSubproject *sub);
    CxString getBaseDirectory(void);

    // Legacy compatibility
    int isLegacyFormat(void);

  private:
    CxString _projectName;
    CxString _baseDirectory;
    CxSList<ProjectSubproject*> _subprojects;

    void loadSubprojects(CxJsonObject *root);
    void loadLegacyFormat(CxJsonObject *root);
};
```

---

## 3. ProjectView UI Changes

### Display Format

```
┌─ Project: Cm Development ─────────────────────┐
│                                               │
│   All                                         │
│                                               │
│ ▼ cx                                          │
│     base/string.cpp                           │
│     buildoutput/buildoutput.cpp               │
│                                               │
│ ▶ cx_tests                                    │
│                                               │
│ ▼ cm (default)                                │
│     Cm.cpp                                    │
│     ScreenEditor.cpp                          │
│     BuildView.cpp                             │
│                                               │
│ [Enter] Open  [m] Make  [c] Clean  [Esc]      │
└───────────────────────────────────────────────┘
```

**Visual elements:**
- `▼` = expanded, `▶` = collapsed
- `(default)` marker on default subproject
- "All" row at top for building all subprojects
- Files indented under their subproject

**Navigation:**
- Arrow keys move through flat list (All + subproject headers + files)
- Enter on "All" row → no action (use m/c to build)
- Enter on subproject header → toggles expand/collapse
- Enter on file → opens it

### ProjectView Data Model

```cpp
enum ProjectViewItemType {
    PVITEM_ALL,           // "All" row at top
    PVITEM_SUBPROJECT,    // Subproject header
    PVITEM_FILE           // File under a subproject
};

struct ProjectViewItem {
    ProjectViewItemType type;
    int subprojectIndex;     // -1 for ALL, 0+ for subprojects/files
    int fileIndex;           // -1 for headers, 0+ for files
};

class ProjectView {
    // ... existing ...

    CxSList<ProjectViewItem*> _visibleItems;  // rebuilt on expand/collapse

    void rebuildVisibleItems(void);
    void toggleSubproject(int subprojectIndex);

    // Get subproject for current selection (for m/c commands)
    // Returns NULL if "All" is selected
    ProjectSubproject* getSelectedSubproject(void);
};
```

---

## 4. ProjectView Quick Commands

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `m` | Make (build) |
| `c` | Clean |
| `t` | Test (make test) |
| Enter | Open file / Toggle expand |
| Arrows | Navigate |
| Esc | Close |

### Context-Sensitive Behavior

| Cursor Position | `m` pressed | `c` pressed | `t` pressed |
|-----------------|-------------|-------------|-------------|
| "All" row | `project-make all` | `project-clean all` | `project-make all test` |
| Subproject header | `project-make <subproject>` | `project-clean <subproject>` | `project-make <subproject> test` |
| File under subproject | `project-make <parent>` | `project-clean <parent>` | `project-make <parent> test` |

### Implementation in ScreenEditor::focusProjectView

ScreenEditor handles the keypress, queries ProjectView for selection:

```cpp
void ScreenEditor::focusProjectView(CxKeyAction keyAction) {
    if (keyAction.actionType() == CxKeyAction::CHARACTER) {
        char ch = keyAction.character();

        if (ch == 'm' || ch == 'M') {
            ProjectSubproject *sub = projectView->getSelectedSubproject();
            if (sub == NULL) {
                startBuildAll("");
            } else {
                startBuild(sub, "");
            }
            showBuildView();
            return;
        }

        if (ch == 'c' || ch == 'C') {
            ProjectSubproject *sub = projectView->getSelectedSubproject();
            if (sub == NULL) {
                startBuildAll("clean");
            } else {
                startBuild(sub, "clean");
            }
            showBuildView();
            return;
        }

        if (ch == 't' || ch == 'T') {
            ProjectSubproject *sub = projectView->getSelectedSubproject();
            if (sub == NULL) {
                startBuildAll("test");
            } else {
                startBuild(sub, "test");
            }
            showBuildView();
            return;
        }
    }

    // Pass other keys to ProjectView for navigation
    projectView->routeKeyAction(keyAction);
}
```

---

## 5. BuildOutput Changes

### Track Active Build Context

```cpp
class BuildOutput {
    // ... existing ...

    CxString _buildDirectory;    // where make was run
    CxString _subprojectName;    // which subproject (for UI)

    void setBuildContext(CxString directory, CxString subprojectName);
    CxString getBuildDirectory(void);
    CxString getSubprojectName(void);

    // For "build all" - append separator line
    void appendSeparator(CxString text);  // adds "=== Building cx ===" line
};
```

### New Line Type

```cpp
enum BuildLineType {
    BUILD_LINE_PLAIN,
    BUILD_LINE_ERROR,
    BUILD_LINE_WARNING,
    BUILD_LINE_NOTE,
    BUILD_LINE_COMMAND,
    BUILD_LINE_SEPARATOR    // "=== Building cx ===" header
};
```

### BuildView Display for Separators

Separator lines displayed with distinct styling (e.g., centered, different color).

---

## 6. ScreenEditor Build Integration

### State

```cpp
class ScreenEditor {
    // ... existing ...

    ProjectSubproject *_activeBuildSubproject;  // current/last build target
    int _buildAllIndex;                         // -1 if not "build all", else current index
    CxString _buildAllTarget;                   // make target for "build all"

    void startBuild(ProjectSubproject *sub, CxString makeTarget);
    void startBuildAll(CxString makeTarget);
    void continueBuildAll(void);  // called when one subproject finishes
};
```

### Build All Flow

1. `project-make all` calls `startBuildAll("")`
2. `startBuildAll` sets `_buildAllIndex = 0`, appends separator "=== Building cx ===", calls `startBuild(subproject[0], target)`
3. In `buildIdleCallback`, when build completes:
   - If `_buildAllIndex >= 0` and exit code == 0 and more subprojects:
     - Increment `_buildAllIndex`
     - Append separator line `"=== Building <next> ==="`
     - Start next build (reuse same BuildOutput, don't clear)
   - If errors (exit code != 0) or last subproject:
     - Set `_buildAllIndex = -1`
     - Finish normally (append "Build Done" line)
4. BuildView shows combined output, errors from any subproject are jumpable

### startBuild Implementation

```cpp
void ScreenEditor::startBuild(ProjectSubproject *sub, CxString makeTarget) {
    _activeBuildSubproject = sub;

    // Get make directory
    CxString makeDir = project->getMakeDirectory(sub);

    // Build command: cd <dir> && make [target]
    CxString command = "cd ";
    command += makeDir;
    command += " && make";
    if (makeTarget.length() > 0) {
        command += " ";
        command += makeTarget;
    }

    // Set build context for error path resolution
    buildOutput->setBuildContext(makeDir, sub->name);

    // Start the build
    buildOutput->start(command);

    _buildStatusPrefix = "Building...";
}
```

---

## 7. Jump-to-Error Resolution

When user presses Enter on an error line in BuildView:

```cpp
void ScreenEditor::jumpToError(BuildOutputLine *line) {
    if (line->filename.length() == 0 || line->line == 0) return;

    // Resolve path using build context
    CxString fullPath;
    CxString buildDir = buildOutput->getBuildDirectory();

    if (buildDir.length() > 0) {
        // Relative to build directory
        fullPath = buildDir;
        fullPath += "/";
        fullPath += line->filename;
        fullPath = CxFileName::canonical(fullPath);
    } else {
        // Legacy: assume relative to current directory
        fullPath = line->filename;
    }

    // Open file and goto line
    openFile(fullPath);
    activeEditView()->gotoLine(line->line);

    // Dismiss BuildView, return to edit
    returnToEditMode();
}
```

**Path resolution in Project:**
```cpp
CxString Project::getMakeDirectory(ProjectSubproject *sub) {
    // baseDirectory + subproject.directory
    CxString path = _baseDirectory;
    path += "/";
    path += sub->directory;
    return CxFileName::canonical(path);
}

CxString Project::resolveFilePath(ProjectSubproject *sub, CxString filename) {
    // baseDirectory + subproject.directory + filename
    CxString path = getMakeDirectory(sub);
    path += "/";
    path += filename;
    return CxFileName::canonical(path);
}
```

---

## 8. Command Changes

### project-make

```
project-make [subproject] [target]
```

| Input | Behavior |
|-------|----------|
| `project-make` | Build default subproject |
| `project-make cx` | Build cx subproject |
| `project-make cx test` | Run `make test` in cx |
| `project-make all` | Build all in order |
| `project-make all test` | Run `make test` on all |

### project-clean

```
project-clean [subproject]
```

| Input | Behavior |
|-------|----------|
| `project-clean` | Clean default subproject |
| `project-clean cx` | Clean cx subproject |
| `project-clean all` | Clean all subprojects |

### Argument Parsing

```cpp
void ScreenEditor::CMD_ProjectMake(CxString args) {
    CxString subprojectName;
    CxString makeTarget;

    // Parse: "[subproject] [target]"
    CxTokenizer tok(args);
    if (tok.hasMore()) {
        subprojectName = tok.next();
    }
    if (tok.hasMore()) {
        makeTarget = tok.next();
    }

    if (subprojectName.length() == 0) {
        // No args: build default
        ProjectSubproject *sub = project->getDefaultSubproject();
        if (sub == NULL) {
            setMessage("(no default subproject)");
            return;
        }
        startBuild(sub, "");
    } else if (subprojectName == "all") {
        startBuildAll(makeTarget);
    } else {
        ProjectSubproject *sub = project->findSubproject(subprojectName);
        if (sub == NULL) {
            setMessage("(unknown subproject)");
            return;
        }
        startBuild(sub, makeTarget);
    }
    showBuildView();
}
```

### Tab Completion for Subproject Names

Add subproject names to completer dynamically:
- When project loaded, register subproject names as completions for project-make/project-clean
- Include "all" as special option
- Could use child completer pattern (like utf-symbol)

---

## 9. Files to Modify

| File | Changes |
|------|---------|
| `Project.h` | Add ProjectSubproject struct, subproject methods, path resolution |
| `Project.cpp` | Parse new JSON format, implement subproject methods, legacy fallback |
| `ProjectView.h` | Add PVITEM_ALL, expand/collapse state, visible items list, getSelectedSubproject() |
| `ProjectView.cpp` | Grouped display with All row, toggle expand/collapse, rebuild visible items |
| `BuildView.h` | (minor) Handle separator line type |
| `BuildView.cpp` | Display BUILD_LINE_SEPARATOR with distinct styling |
| `buildoutput.h` | Add BUILD_LINE_SEPARATOR, build context fields (directory, subproject name) |
| `buildoutput.cpp` | setBuildContext(), appendSeparator(), getBuildDirectory() |
| `ScreenEditor.h` | Add _activeBuildSubproject, _buildAllIndex, _buildAllTarget, build methods |
| `ScreenEditor.cpp` | startBuild(), startBuildAll(), continueBuildAll(), jumpToError() with path resolution |
| `ScreenEditorCommands.cpp` | Parse subproject/target args in CMD_ProjectMake/Clean |
| `focusProjectView` | Handle m/c/t quick commands |

---

## 10. Implementation Order

1. **Project.h/cpp** - New data structures, JSON parsing, path resolution, legacy fallback
2. **ProjectView.h/cpp** - Add "All" row, grouped display with expand/collapse
3. **buildoutput.h/cpp** - Build context (directory, name), separator line type
4. **BuildView.cpp** - Display separator lines with distinct styling
5. **ScreenEditor** - Single subproject builds with context tracking
6. **Jump-to-error** - Use build directory for path resolution
7. **Build all** - Sequential builds with separators, stop on error
8. **ProjectView quick commands** - m/c/t keys in focusProjectView
9. **Command args** - Parse subproject/target in CMD_ProjectMake/Clean
10. **Tab completion** - Dynamic subproject name completion (optional, can defer)

---

## 11. Testing Checklist

1. [ ] Load legacy project file - should work unchanged
2. [ ] Load new format - subprojects parsed correctly
3. [ ] ProjectView shows "All" row at top
4. [ ] ProjectView shows subproject groups with expand/collapse
5. [ ] Enter on subproject header toggles expand/collapse
6. [ ] Enter on file opens it with correct path
7. [ ] `m` key on "All" builds all subprojects
8. [ ] `m` key on subproject header builds that subproject
9. [ ] `m` key on file builds parent subproject
10. [ ] `c` key cleans appropriately
11. [ ] `t` key runs tests appropriately
12. [ ] `project-make` builds default subproject
13. [ ] `project-make cx` builds named subproject
14. [ ] `project-make cx test` runs make test
15. [ ] `project-make all` builds in order with separator lines
16. [ ] Build all stops on first error
17. [ ] Jump-to-error resolves paths correctly for each subproject
18. [ ] BuildView title shows which subproject is building
19. [ ] Resize while ProjectView open - layout recalculates
20. [ ] Resize while BuildView open - layout recalculates

---

## 12. Example Project File for Testing

Save as `cm.project` after implementation:

```json
{
  "projectName": "Cm Development",
  "baseDirectory": "../..",
  "subprojects": [
    {
      "name": "cx",
      "directory": "cx",
      "makefile": "makefile",
      "files": [
        "base/string.cpp",
        "base/string.h",
        "screen/screen.cpp",
        "screen/screen.h",
        "screen/boxframe.cpp",
        "screen/boxframe.h",
        "buildoutput/buildoutput.cpp",
        "buildoutput/buildoutput.h",
        "process/process.cpp",
        "process/process.h"
      ]
    },
    {
      "name": "cx_tests",
      "directory": "cx_tests",
      "makefile": "makefile",
      "files": [
        "cxbuildoutput/cxbuildoutput_test.cpp",
        "cxbuildoutput/makefile"
      ]
    },
    {
      "name": "cm",
      "directory": "cx_apps/cm",
      "makefile": "makefile",
      "default": true,
      "files": [
        "Cm.cpp",
        "ScreenEditor.cpp",
        "ScreenEditor.h",
        "ScreenEditorCommands.cpp",
        "ScreenEditorCore.cpp",
        "EditView.cpp",
        "EditView.h",
        "CommandLineView.cpp",
        "CommandLineView.h",
        "ProjectView.cpp",
        "ProjectView.h",
        "BuildView.cpp",
        "BuildView.h",
        "Project.cpp",
        "Project.h"
      ]
    }
  ]
}
```

---

## 13. Design Decisions Summary

| Decision | Choice |
|----------|--------|
| File in multiple subprojects? | Allowed (navigation convenience, not build deps) |
| Subproject-specific make targets? | Yes: `project-make <sub> <target>` |
| Build order for "all"? | JSON array order |
| Build all behavior on error? | Stop, don't continue to next subproject |
| Build all output? | Append with separator lines "=== Building cx ===" |
| ProjectView grouping? | Headers with expand/collapse |
| Quick commands in ProjectView? | m=make, c=clean, t=test |
| Quick command context? | Based on cursor: All row, subproject, or file's parent |
