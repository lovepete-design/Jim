# Hex Library sound library

Hex Library 0.2.1 keeps your DX7 sounds inside the plugin. Import your main sound folder once, including all its subfolders. Browse and switch voices directly in Hex after that.

## Install this update on macOS

1. Quit Ableton Live.
2. Unzip **Hex-Library-macOS.zip** and open the extracted **hex-macos** folder.
3. Open **Terminal** (press Command-Space, type Terminal, then Return).
4. Type `bash ` with a space after it. Drag **install-hex-macos.sh** from the extracted folder into Terminal, then press Return.
5. Reopen Ableton and load **Hex Library** on a MIDI track.

The installer updates Hex Library in `~/Library/Audio/Plug-Ins/VST3/` and saves the previous version under `.triad-backups` there. It does not need a password. Existing Stress and Lattice installations are untouched. The plugin has a local ad hoc signature; the installer checks that signature and clears the download quarantine on this plugin bundle only.

If Live keeps showing the old interface, open **Settings > Plug-Ins**, enable **Use VST3 Plug-In System Folders**, then hold **Option** while clicking **Rescan**.

## Import and browse

1. Click **Import Folder** and choose your top-level sound folder.
2. Wait for the import to finish. Hex reads supported `.syx` and `.sysex` files in that folder and all its subfolders.
3. Use the folder selector to narrow the list, or type a voice name in search.
4. Click a sound to play it. Use the previous/next buttons, or the list's up/down arrow keys, to move through sounds without opening Finder.

Use **Add Files** to bring in individual files. A normal DX7 32-voice bank adds all 32 voices to the browser. Invalid or unsupported files are skipped; the import summary reports them. Reimporting the same collection avoids adding the same entries twice.

Imported voices are copied into Hex's own library, with their folder organization retained. You can move or remove the original download folders after import. The library remains available when you close the plugin or restart Live. Use **Refresh** in another open Hex instance after importing sounds elsewhere.

The library is stored in:

`~/Library/Application Support/Triad Audio/Hex MkI/Sound Library/`

Back up that folder to keep your collection or move it to another Mac. Each Ableton Set also saves the complete currently selected voice, so it can recall that sound even when the library is unavailable. The three original starting presets remain available in the preset menu.

## Source and licensing

The matching build source is included as **triad-audio-source.tar.gz**, together with license notices. Imported third-party sound collections keep their own licenses. This update does not include Yamaha factory banks.

Hex Library installs alongside Hex MkI with a separate plugin identity. Existing Sets continue using Hex MkI. Both versions share the imported sound library.
