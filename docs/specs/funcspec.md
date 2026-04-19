# The WhatToWipe Utility. Functional Specification

## Purpose

The WhatToWipe utility helps users see how disk space is used. It shows how large each part is next to the full volume, without guesswork. It supports clear choices about what to keep, move, or remove.


## Concepts and Terms

*Program:* If not specified, the WhatToWipe utility.

*Volume:* A storage unit recognized by the operating system as a single accessible location, such as a physical drive, logical drive, partition, or mounted storage, excluding network locations.

*Volume capacity:* The total capacity of the current volume.

*Volume free space:* Free space on the current volume.

*Target folder:* A folder that has been chosen by the user as a root.

*Context folder:* A folder that the user is currently exploring.

*Current volume:* The volume that contains the context folder.

*Subfolder:* If not specified, a folder within the context folder.

*Node subfolder:* A subfolder that contains nested folders.

*Leaf subfolder:* A subfolder that does not contain nested folders.

*Superfolder:* If not specified, a folder that directly contains the context folder.

*Folder class:* The indicator telling whether the folder is node or leaf.

*Size of the folder:* The aggregated size of all the files inside the folder, calculated recursively.

*Volume share of the folder:* The share of the folder size compared to the total capacity of the volume.

*Scanning a folder:* An automatic activity during which the program collects data on the folder, its nested folders, and files recursively.

*Success:* Applied to scanning a folder, means that the scanning was not interrupted by the user voluntarily or by the program for technical reasons.

> **NOTE**
> If the program fails to collect data on some inner folders or files because of insufficient permicions, but finishes scanning organically without interruptions it's still success.   

*Treemap:* A diagram that represents the content of the context folder, its structure, and the properties of its subfolders, including their sizes and volume shares.

*Complete treemap:* A treemap representing a successfully scanned folder.

**Unset treemap:* A treemap that is not complete. 

*Tile:* One region of the treemap; each tile represents a subfolder of the context folder.

*Navigation:* The user moves up and down the folder hierarchy in the program interface by changing the context folder to bring a folder of interest into focus.


## Use Cases

### Available Use Cases

The program must support the following use cases:

- *Starting a New Session*
- *Choosing a Target Folder*
- *Scanning the Context Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Exploring the Context Folder*
- *Exploring a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Starting a New Session

**Context**

The user decides to clean up volumes on their devices.

**Steps**

1. The user launches the program.

   **Step Result**

   The program starts. The main window opens.

**Result**

- The program is ready to work.
- The treemap is unset.
- The volume indicators are unset.

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

   The system **Open** dialog box opens.

2. The user selects a folder.

   2 Positive. The program accepts the selected folder.

   **Conditions**

   The folder is available for scanning. 

   **Step Result**

   - The folder selected by the user becomes a target folder.
   - The folder selected by the user becomes a context folder.
   - The volume indicators are updated.
   - The treemap gets unset.
   
   2 Negative. The program rejects the selected folder. 

   **Conditions**

   The folder if unavailable because of some technical reasons. 
   
   The possible reasons for rejecting the folder are listed below.
   
   - The folder has been deleted.
   - The program does not have sufficient permissions to scan the folder.
   - Other runtime errors or technical reasons. 

   **Step Results**

   An error code is #001. The program displays an error alert.   

3. The program starts the use case *Scanning the Context Folder*.

   **Step Results**

   The use case *Scanning the Context Folder* has strted.
   
4. The program handles the scanning outcomes.

   4 Positive. The treemap gets complete.

   **Conditions**

   The target folder has been fully scanned.

   **Step Result**

   The treemap is displayed complete.
   
   4 Negative. The treemap gets unset.

   **Conditions**

   - User interrupted scanning of the target folder.  
     OR  
   - Scanning of the target folder failed by other reasons.

   **Step Result**

   The treemap is displayed unset.

**Result**

   **Positive**

   The treemap is displayed complete.
   
   **Negative**

   The treemap is displayed unset.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Exploring the Context Folder*
- *Exploring a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Updating the Context Folder

**Context**

The treemap is based on data collected during the last scan. This data includes folder sizes, structure, and volume shares. Changes in the file system make this data outdated. Files and folders may be deleted, moved, or added. The treemap does not update automatically. It may no longer reflect the actual state of the context folder. This use case updates the data and rebuilds the treemap.

**Prerequisites**

- A context folder is set,  
  AND  
- The program is not scanning.

**Steps**

1. The user selects the **File → Update** command.

   **Step Result**

   The context folder is declared *anchored*.

   The use case *Scanning the Context Folder* starts.

2. The program handles the scanning outcomes.

   2 Positive. The treemap gets complete.

   **Conditions**

   The context folder has been fully scanned.

   **Step Result**

   - The treemap is displayed complete.
   - The **Free** indicator is updated.

   2 Negative. The treemap gets unset.

   **Conditions**

   - The user interrupted the scanning,
     OR  
   - The scanning failed for other reasons.

   **Step Result**

   The treemap is displayed unset.

**Result**

   **Positive**

   An *appropreate* folder is identified according to the table below.

   | Target Folder | Anchored Folder | Context Folder | Appropreate Folder |
   |---------------|-----------------|----------------|--------------------|
   | Exists        | Exists          | Exists         | Context folder     |
   | Exists        | Exists          | Absent         | Pinned folder      |
   | Exists        | Absent          | Absent         | Target folder      |
   | Exists        | Absent          | Exists         | Context folder     |
   | Absent        | Absent          | Absent         | Not identified     |

   - A complete treemap is shown for the appropreate folder if it is identified.
   - An unset treemap is shown if an appropreate folder in not identified. 
   - The the **Free** indicator is updated.

   **Negative**

   The treemap is displayed unchanged.

**Rules**

The user is allowed to navigate while the use case *Scanning the Context Folder* is in progress.

**Postrequisites**

The following use cases are available to the user:

- *Choosing a Target Folder*
- *Updating the Context Folder*
- *Diving Into a Subfolder*
- *Returning to a Superfolder*
- *Exploring the Context Folder*
- *Exploring a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Scanning the Context Folder

**Context**

- Overall use case *Choosing a Target Folder*, step 2,  
  OR  
- Overall use case *Updating the Context Folder*, step 1.

**Steps**

1. The program begins to scan the context folder hierarchy.

   **Step Result**

   - The **Inspetc → Update** command disappears.
   - The **Inspect → Stop** command appears.

2. The user waits or reacts.

   2 Positive. User waits when the scanning has finished.

   **Step Result**

   The scanning has finished successfully.

   2 Negative #1. The user selects the **Inspect → Stop** command.

   **Step Result**

   The scanning is terminated.

   2 Negative #2. An error occurs during the scanning.

   **Step Result**

   The scanning is terminated.

3. The scanning finishes.

   **Step Result**

      **Any Case**

      - The **Inspetc → Stop** command disappears.
      - The **Inspect → Update** command appears.
      
   **Positive**

      The exit code `success` returns to the overall use case.

   **Negative**

      The exit code `terminated` returns to the overall use case.  

**Rules**

All nested folders and files must be scanned recursively from the context folder down to terminal folders and files.

During scanning, the path to the folder that is currently being scanned must be displayed in the status bar as a full path.

The following data must be collected or updated for the context folder and for each folder within it:

- Name
- Full path
- Folder size
- Volume share of the folder
- Class of the folder

The following numeric data must be recalculated along the entire hierarchy if the context folder is not the target folder:

- Folder sizes
- Volume shares of the folders

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
   - If the **current volume** differs from the volume previously shown, then the following indicators must be updated:
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
- *Exploring the Context Folder*
- *Exploring a Subfolder*
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
   - If the **current volume** changes compared to the previous context folder, then the following indicators must be updated:
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
- *Exploring the Context Folder*
- *Exploring a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Exploring the Context Folder

**Context**

The user wants to open the context folder in the system file manager.

**Prerequisites**

- A context folder is set.

**Steps**

1. The user clicks the **Explore** button.

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
- *Exploring the Context Folder*
- *Exploring a Subfolder*
- *Displaying the Program Information*
- *Quitting the Current Session*


### Exploring a Subfolder

**Context**

The user wants to open a subfolder in the system file manager.

**Prerequisites**

- A treemap is displayed complete.

**Steps**

1. The user right-clicks a tile.

   **Step Result**

   A context menu opens.

2. The user selects the **Inspect → Explore** command.

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
- *Exploring the Context Folder*
- *Exploring a Subfolder*
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
- *Exploring the Context Folder*
- *Exploring a Subfolder*
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

| Command                  | Toolbar     | Hot Keys      | Action                               |
|--------------------------|-------------|---------------|--------------------------------------|
| **File → Open a Folder** | **Open**    | **Ctrl+O**    | *Choosing a Target Folder*           |
| **File → Exit**          |             | **Ctrl+X**    | *Quitting the Current Session*       |
| **Inspect → Up**         | **Up**      | **Backspace** | *Returning to a Superfolder*         |
| **Inspect → Explore**    | **Explore** | **Ctrl+E**    | *Exploring the Context Folder*       |
| **Inspect → Update**     | **Update**  | **Ctrl+S**    | *Updating the Context Folder*        |
| **Inspect → Stop**       | **Stop**    | **Esc**       | Terminating scanning                 |
| **Help → About**         |             |               | *Displaying the Program Information* |

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

The phrase *command gets disabled* refers to the following effects arising at the same time.

The corresponding menu item is displayed as disabled.
The corresponding button in the toolbar is displayed as disabled.
The corresponding hotkeys do not produce the corresponding response.

The phrase *command gets enabled* means that the effects listed above cease at the same time.

The phrase *command disappears* refers to the follwoing effects arising at the same time. 

- The corresponding item disappears from the menu.
- The corresponding button disappears from the toolbar.
- The corresponding hotkeys do not produce the corresponding response. 

The phrase *command appears* means that the effects listed above cease at the same time. 


### Menu

The menu must have the following structure.

- **File**
  - **Open a Folder**
  - --- (separator)
  - **Exit**
- **Inspect**
  - **Up**
  - **Explore**
  - --- (separator)
  - **Update**
  - **Stop**
- **Help**
  - **About**


### Toolbar

#### Toolbar Elements

The toolbar must contain the following elements.

| Element     | Type      | Icon/Data          | Tooltip                        | Label         |
|-------------|-----------|--------------------|--------------------------------|---------------|
| **Open**    | Button    | Folder icon        | `Open a folder`                |               |
| **Up**      | Button    | Top-pointing arrow | `Go up`                        |               |
| **Explore** | Button    | Eye icon           | `Open in file manager`         |               |
| **Update**  | Button    | Green play icon    | `Update the folder data`       |               |
| **Stop**    | Button    | Stop indicator     | `Stop scanning folders`        |               |
| **Total**   | Indicator | Volume capacity    | `Total capacity of the volume` | `Total at X:` |
| **Free**    | Indicator | Volume free space  | `Free space on the volume`     | `Free at X:`  |

The **Total** and **Free** indicators are the *volume indicators* together.


#### Indicator **Total** 

The **Total** element must be implemented as a static text. The static text must be `Total at X:`. The `X` stands for the volume label (**current volume** letter or name); the text must also show total capacity for that volume.


#### Indicator **Free** 

The **Free** element must be implemented as a compound that contains the following nested elements:

- Static label
- Button 

The static label must be *Free at X:*. The *X* stands for the current volume letter or name. 

The button must display the volume free space. When the user clicks on the button the volume free space updates. 


#### Icons

Toolbar button icons must be rasterized at 24×24 pixels in the toolbar’s image list at 96 DPI. The implementation scales this nominal size with the toolbar host DPI so icons remain sharp on high-DPI displays. Icons must remain visually distinct and legible at that size.


### Treemap

#### Layout

The treemap must always be stretched over the client area. The treemap must get resized accordingly when the user resizes the main window.


#### Representation

Each tile of the treemap must represent a subfolder.

The size of the tile must correspond to the volume share of the subfolder.


#### Tile Colors

Tiles that represent node subfolders must be displayed colored. The colors that are allowed for painting tiles are listed below. 

| Color Name   | sRGB Hex  |
|--------------|-----------|
| Atoll        | `#AFE9DE` |
| Blush        | `#EFBFD4` |
| Foxglove     | `#DCC8F2` |
| Nectarine    | `#FFD4BF` |
| Peridot      | `#C9ECC5` |
| Quince       | `#F2E2B3` |
| Stratosphere | `#B8DFF7` |

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
| Volume share   | `Folder Details` | One fractional digit; `%` |

The file object size must include the following items: 

- One fractional digit
- White space 
- One suffix `TB`, `GB`, `MB`, or `KB`; pick largest fitting unit. 

Zero must give `0.0 KB` for a file object size.

A shabby tile must show the same three-line label (same data and formats as in the table above). **Folder Name** must use **10 points**. **Folder Details** must use **8 points** (0.8 × 10 per the style table). Text that would extend past the tile is clipped at the tile boundary.


#### Tooltips

Shabby tiles already show the three-line on-tile label (see **Tile Labels**). **Hover tooltips for shabby tiles are not required in the current product build** (they were removed to avoid treemap repaint artifacts). A future build may restore optional shabby hover tooltips that repeat the same three lines.


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


## Handling Errors

### Reasons for Handling Errors

Some failures must be reported explicitly so the user understands why an action did not complete and what they can do next.


### Errors

Every error has the following properties:

- Code
- Reason
- Message

The code identifies the failure. The same class of failure keeps the same code across releases unless this specification changes. The code is suitable for documentation and for support references.

The reason states why the program assigns the code. It describes the technical or situational conditions that qualify the failure. The reason is part of the specification text in the table below.

The message is user-facing text paired with the code for that failure. It must convey the meaning defined for that code in the table below. Other languages may use different wording. The English *Message* column in the table still defines what each code must communicate.

The table below lists the value of each property for every defined failure.

| Code   | Reason | Message |
|--------|--------|---------|
| `001` | The user chose a folder that cannot be used as the scan root. | The folder could not be opened for scanning. Check that it still exists and that you have permission to read it. |
| `002` | Scanning did not finish successfully for a technical reason. | Scanning stopped because of an error. The treemap may be incomplete or empty. |
| `003` | The folder could not be opened in the system file manager. | The folder could not be opened in File Explorer. |


### Error Alert

The program opens error alerts to inform the user about an error. 

An error alert must be a modal dialog box. 

An error alert must contain the following elements:

- Error alert icon
- Error info block
- **OK** button

The error alert icon must be a yellow rectangle with a black exclamation mark inside. 

An error info block must provode the following data: 

- Error code
- Error message

The **OK** button must hold the focus.

An error alert must get closed when the user performs one of the following actions: 

- Closes the error alert using the standard operating system method
- Clicks on the **OK** button.
- Presses one of the following keys:
   - **Enter**
   - **Esc**