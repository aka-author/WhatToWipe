# The WhatToWipe Utility. Functional Specification

## Purpose

The WhatToWipe utility helps users see how disk space is used. It shows how large each part is next to the full drive, without guesswork. It supports clear choices about what to keep, move, or remove.


## Concepts and Terms

**Program:** The WhatToWipe utility.

**Target folder:** A folder that has been opened by the user as a root.

**Context folder:** A folder that the user is currently exploring.

**Subfolder:** A folder within the context folder.

**Node subfolder:** A subfolder that contains nested folders.

**Leaf subfolder:** A subfolder that does not contain nested folders.

**Superfolder:** A folder that directly contains the context folder.

**Folder class:** The indicator telling whether the folder is node or leaf.

**Size of the folder:** The aggregated size of all the files inside the folder, calculated recursively.

**Drive share of the folder:** The share of the folder size compared to the total capacity of the drive.

**Treemap:** A diagram made of **tiles** that displays the content of the context folder. If the context folder has subfolders, then the treemap displays their sizes, drive shares, and other helpful information. A treemap is **complete** when it represents a folder that has been scanned successfully. A treemap is **unset** otherwise.

**Tile:** One region of the treemap; each tile represents a subfolder of the context folder.

**Navigation:** Moving up and down the folder hierarchy to bring a folder of interest into focus.

**Current drive:** the volume that contains the context folder.

**Drive capacity:** The total capacity of the current drive.

**Drive free space:** Free space on the current drive.


## Use Cases

### Available Use Cases

The program must support the following use cases:

- *Starting a New Session*
- *Choosing a Target Folder*
- *Scanning the Context Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Starting a New Session

**Context**

The user decides to clean up drives on their devices.

**Steps**

1. The user launches the program.

   The program starts. The main window opens.

**Result**

- The program is ready to work.
- The treemap zone is unset.
- The following indicators must be updated:
  - **Total**
  - **Free**

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Choosing a Target Folder

**Context**

The user has to choose a folder that they will treat as a target folder.

**Prerequisites**

The program is not scanning.

**Steps**

1. The user selects the **File → Open a Folder** command.

   **Step Result**

   A system **Open** dialog box opens.

2. The user selects a folder.

   **Step Result**

   - The folder selected by the user becomes the target folder and the context folder.
   - The treemap gets unset.
   - The use case *Scanning the Context Folder* starts.
   - The following indicators must be updated:
     - **Total**
     - **Free**

3. The program handles the scanning outcomes.

   3 Positive. The treemap gets complete

   **Conditions**

   The target folder has been fully scanned.

   **Step Result**

   - The treemap is displayed complete.
   - The following indicators must be updated:
     - **Total**
     - **Free**

   3 Negative. The treemap gets unset

   **Conditions**

   - User interrupted scanning of the target folder.  
     OR  
   - Scanning of the target folder failed by other reasons.

   **Step Result**

   - The treemap is displayed unset.
   - The following indicators must be updated:
     - **Total**
     - **Free**

**Result**

   **Positive**

   - The treemap is displayed complete.
   - The following indicators must be updated:
     - **Total**
     - **Free**

   **Negative**

   - The treemap is displayed unset.
   - The following indicators must be updated:
     - **Total**
     - **Free**

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Updating the Context Folder

**Context**

The treemap is based on data collected during the last scan. This data includes folder sizes, structure, and drive shares. Changes in the file system make this data outdated. Files and folders may be deleted, moved, or added. The treemap does not update automatically. It may no longer reflect the actual state of the context folder. This use case updates the data and rebuilds the treemap.

**Prerequisites**

- A context folder is set,  
  AND  
- The program is not scanning.

**Steps**

1. The user clicks the **Update** button.

   **Step Result**

   The use case *Scanning the Context Folder* starts.

2. The program handles the scanning outcomes.

   2 Positive. The treemap gets complete

   **Conditions**

   The context folder has been fully scanned.

   **Step Result**

   - The treemap is displayed complete.
   - The following indicators must be updated:
     - **Total**
     - **Free**

   2 Negative. The treemap gets unset

   **Conditions**

   - The user interrupted scanning.  
     OR  
   - Scanning failed for other reasons.

   **Step Result**

   - The treemap is displayed unset.
   - The following indicators must be updated:
     - **Total**
     - **Free**

**Result**

   **Positive**

   - The treemap is displayed complete.
   - The following indicators must be updated:
     - **Total**
     - **Free**

   **Negative**

   - The treemap is displayed unset.
   - The following indicators must be updated:
     - **Total**
     - **Free**

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Scanning the Context Folder

**Context**

- Overall use case *Choosing a Target Folder*, step 2.  
  OR  
- Overall *Updating the Context Folder*, step 1.

**Steps**

1. The program begins to scan the context folder hierarchy.

   **Step Result**

   - The **Update** button disappears.
   - The **Stop** button appears.

2. User reacts.

   2 Positive. User waits when the scanning has finished

   **Step Result**

   The scanning has finished successfully.

   2 Negative #1. The user clicks on the **Stop** button

   **Step Result**

   The scanning is terminated.

   2 Negative #2. An error occurs during the scanning

   **Step Result**

   The scanning is terminated.

3. The scanning finishes.

   **Step Result**

   - The **Stop** button disappears.
   - The **Update** button appears.
   - If the scan finished **successfully** and the treemap is **complete**, then the following indicators must be updated:
     - **Total**
     - **Free**
   - If the scan did **not** finish successfully, follow the negative indicator rules of the calling use case. That use case is either *Choosing a Target Folder* or *Updating the Context Folder*.

**Rules**

All nested folders and files must be scanned recursively from the context folder down to terminal folders and files.

During scanning, the path to the folder that is currently being scanned must be displayed in the status bar as a full path.

The following data must be collected or updated for the context folder and for each folder within it:

- Name
- Full path
- Folder size
- Drive share of the folder
- Class of the folder

The following numeric data must be recalculated along the entire hierarchy if the context folder is not the target folder:

- Folder sizes
- Drive shares of the folders

The data collected during the scanning must be stored in memory and used for navigation within the target folder.

While a scan is in progress, the following indicators must be updated as required for the scan root:
- **Total**
- **Free**

**Result**

   **Positive**

   Data on the target folder and the descendants is collected.

   **Negative**

   Data is not collected because of the termination.

**Postrequisites**

The overall use case proceeds.


### Diving Into a Subfolder

**Prerequisites**

- A treemap is displayed complete.

**Steps**

1. The user clicks on a tile.

2. The program verifies the tile corresponds to a node subfolder.

3. The program handles the click.

   3.POS — The program displays a subfolder

   **Conditions**

   The subfolder corresponding to the tile is node.

   **Step Result**

   - The subfolder corresponding to the tile becomes a context folder.
   - The treemap is updated for the new context folder.
   - If the **current drive** (volume that contains the new context folder) differs from the one previously shown, then the following indicators must be updated:
     - **Total**
     - **Free**

**Result**

   **Positive**

   New context folder is set.

   **Negative**

   The click is ignored.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Returning to a Superfolder

**Context**

The user navigates the folder hierarchy upward.

**Prerequisites**

- A treemap is displayed complete,  
  AND  
- The context folder is not the target folder.

**Steps**

1. The user clicks the **Up** button or presses **Backspace** key.

   **Step Result**

   - The superfolder becomes the context folder.
   - The treemap is updated for the new context folder.
   - If the **current drive** changes compared to the previous context folder, then the following indicators must be updated:
     - **Total**
     - **Free**

**Result**

The treemap is displayed for the superfolder.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Managing the Context Folder

**Context**

The user wants to open the context folder in the system file manager.

**Prerequisites**

- A context folder is set.

**Steps**

1. The user clicks the **Manage** button.

   **Step Result**

   A system file manager window opens for the context folder.

**Result**

The context folder is opened in the system file manager.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Managing a Subfolder

**Context**

The user wants to open a subfolder in the system file manager.

**Prerequisites**

- A treemap is displayed complete.

**Steps**

1. The user right-clicks a tile.

   **Step Result**

   A context menu opens.

2. The user selects the **Inspect** command.

   **Step Result**

   A system file manager window opens for the subfolder.

**Result**

The selected subfolder is opened in the system file manager.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Quitting the Current Session

**Context**

The user finishes working with the program.

**Prerequisites**

The program is not scanning folders.

**Steps**

1. The user selects the **File → Exit** command.

   **Step Result**

   The program exits.

**Result**

The session is finished.

**Postrequisites**

None.


### Displaying the Program Information

**Context**

The user wants to see information about the program.

**Prerequisites**

None.

**Steps**

1. The user selects the **Help → About** command.

   **Step Result**

   The **About** dialog opens.

2. The user closes the About dialog.

   **Step Result**

   The **About** dialog closes.

**Rules**

While the **About** dialog is open, the following text must be visible:

- The product name **WhatToWipe**.
- The application version as a single dotted string **Major.Minor.Patch.Build**, where Major, Minor, Patch, and Build are decimal integers and the string is identical to the **File version** value shown in Windows File Explorer for the running executable (Properties → Details).

**Result**

The **About** dialog has opened and shown the text required in **Rules**, then closed.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Managing the Context Folder*
- *Managing a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


## User Interface

### Main Window

The main window must contain the following elements:

- Main menu
- Toolbar
- Treemap
- Status bar

The main window must have a unique icon.


### Commands

The following commands must be available for the user in the main window.

| Command                  | Toolbar    | Hot Keys      | Action                               |
|--------------------------|------------|---------------|--------------------------------------|
| **File → Open a Folder** | **Open**   | **Ctrl+O**    | *Choosing a Target Folder*           |
| **File → Exit**          |            | **Ctrl+X**    | *Quitting the Current Session*       |
| **Inspect → Up**         | **Up**     | **Backspace** | *Returning to a Superfolder*         |
| **Inspect → Manage**     | **Manage** | **Ctrl+M**    | *Managing the Context Folder*        |
| **Inspect → Update**     | **Update** | **Ctrl+S**    | *Updating the Context Folder*        |
| **Inspect → Stop**       | **Stop**   | **Esc**       | Terminating scanning                 |
| **Help → About**         |            |               | *Displaying the Program Information* |

Selecting menu commands, clicking on toolbar buttons, and pressing hot keys must be treated as equivalent actions according to the table above. 

> **NOTE**
> In this document, use case descriptions must refer only to menu commands for the sake of brevity.

These rules must be fulfilled at any moment. Elements corresponding to unavailable actions are disabled. Elements corresponding to available actions are enabled.

The following groups of commands must be treated as mutually exclusive:

- Group *UpdateStop*: **Inspect → Update**, **Inspect → Stop**

The following rules must be implemented for the mutually exclusive elements: 

* Mutually exclusive elements never visible at the same time. 
* Mutually exclusive elements hold the same place. 

The mutually exclusive groups must affect the following access methods:

- Menu commands
- Toolbar elements
- Hot keys


### Menu

The menu must have the following structure.

- **File**
  - **Open a Folder**
  - --- (separator)
  - **Exit**
- **Inspect**
  - **Up**
  - **Manage**
  - --- (separator)
  - **Update**
  - **Stop**
- **Help**
  - **About**


### Toolbar

#### Toolbar Elements

The toolbar must contain the following elements.

| Element    | Type      | Icon/Data          | Tooltip                       | Label         |
|------------|-----------|--------------------|-------------------------------|---------------|
| **Open**   | Button    | Folder icon        | *Open a folder*               |               |
| **Up**     | Button    | Top-pointing arrow | *Go up*                       |               |
| **Manage** | Button    | Eye icon           | *Open in file manager*        |               |
| **Update** | Button    | Green play icon    | *Update the folder data*      |               |
| **Stop**   | Button    | Stop indicator     | *Stop scanning folders*       |               |
| **Total**  | Indicator | Drive capacity     | *Total capacity of the drive* | *Total at X:* |
| **Free**   | Indicator | Drive free space   | *Free space on the drive*     | *Free at X:*  |


#### Indicator **Total** 

The **Total** element must be implemented as a static text. The static text must be *Total at X:*. The *X* stands for the volume label (**current drive** letter or name); the text must also show total capacity for that volume.


#### Indicator **Free** 

The **Free** element must be implemented as a compound that contains the following nested elements:

- Static label
- Button 

The static label must be *Free at X:*. The *X* stands for the current drive letter or name. 

The button must display the drive free space. When the user clicks on the button the drive free space updates. 


#### Icons

Toolbar button icons must be rasterized at 24×24 pixels in the toolbar’s image list at 96 DPI. The implementation scales this nominal size with the toolbar host DPI so icons remain sharp on high-DPI displays. Icons must remain visually distinct and legible at that size.


### Treemap

#### Layout

The treemap must always be stretched over the client area. The treemap must get resized accordingly when the user resizes the main window.


#### Representation

Each tile of the treemap must represent a subfolder.

The size of the tile must correspond to the drive share of the subfolder.


#### Tile Colors

Tiles that represent node subfolders must be displayed colored. The colors that are allowed for painting tiles are listed below. 

| Color Name | sRGB Hex  |
|------------|-----------|
| Amber      | `#D97706` |
| Blue       | `#2563EB` |
| Green      | `#16A34A` |
| Rose       | `#E11D48` |
| Slate      | `#475569` |
| Teal       | `#0D9488` |
| Violet     | `#7C3AED` |

Tiles that represent leaf subfolders must be displayed grayscale.


#### Fonts 

The **Headline font** shall be `Segoe UI`.


#### Metrics

The following metrics are defined for a tile:

* Extended length
* Horizontal unit
* Vertical unit

The **extended length** is the number of characters in the folder name plus two.

The **horizontal unit (hu)** is the width of the tile in points divided by the extended length.

The **vertical unit (vu)** is the **Headline font** size at which the character `M` is as wide as one horizontal unit.

A vertical unit is not allowed to be larger that 45 points. If any calculation yields a larger value, then the vertical unit must be clamped to this limit.


#### Styles

The following styles are recognized:

* `Folder Name`
* `Folder Details`

The following properties are defined for the `Folder Name`.

| Property     | Value        |
|--------------|--------------|
| Indent above | 0            |
| Font name    | Headline font |
| Font size    | 1 vu         |
| Bold         | No           |
| Italic       | No           |

The following properties are defined for the `Folder Details`.

| Property     | Value         |
|--------------|---------------|
| Indent above | 0.5 vu        |
| Font name    | Headline font |
| Font size    | 0.8 vu        |
| Bold         | No            |
| Italic       | No            |


#### Tile Classes

The following classes of tiles are recognized:

* Fancy
* Shabby

The tile is **fancy** if the following conditions are true for it at the same time: 

* Its vertical unit is not less than 10 points. 
* Its height is not less than 5 vertical units. 

Otherwise the tile is **shabby**. 


#### Tile Labels

A fancy tile must show a label consisting of the following lines.

| Data          | Style            | Format                    |
|---------------|------------------|---------------------------|
| Folder name   | `Folder Name`    | Plain text                |
| Folder size   | `Folder Details` | File object size          |
| Drive share   | `Folder Details` | One fractional digit; `%` |

The file object size must include the following items: 

- One fractional digit
- White space 
- One suffix `TB`, `GB`, `MB`, or `KB`; pick largest fitting unit. 

Zero must give `0.0 KB` for a file object size.

A shabby tile must not show a label. 


#### Tooltips

On a shabby tile, the program must show the same three lines in a tooltip. 

The tooltip must show the same data a label would show for the same tile.


### Status Bar

Basically, the status bar is used to display the current status.

When the treemap is unset and the program is not scanning, the phrase *Choose a target folder* must be displayed.

When the treemap is complete, the path to the context folder must be displayed.


### System Open Dialog Box

The open folder dialog box must include the following buttons:

- **Open**
- **Cancel**

> **IMPORTANT**  
> Button Create New Folder is irrelevant.
