# Xbox (Developer Mode)

An experimental build of lba2cc runs on an Xbox in Developer Mode. You install it, and copy your game files to it, from a web browser on your computer; no other tools are needed. It has been played on an Xbox Series S. No game files are included: you need your own copy of Little Big Adventure 2.

You need:

- An Xbox One or Xbox Series X|S. Developer Mode is Microsoft's own feature for running apps that are not from the Store; while the console is in it, Store games do not run, and you can switch back at any time.
- A computer with a web browser, on the same network as the Xbox.
- Your copy of the game installed on that computer, for example the 2022 "Classic" edition from Steam or GOG.

## 1. Put the Xbox in Developer Mode (once)

Follow [Microsoft's steps](https://learn.microsoft.com/previous-versions/windows/uwp/xbox-apps/devkit-activation). In short:

1. On the Xbox, install the **Xbox Dev Mode** app from the Microsoft Store and open it. It shows a code.
2. On the computer, create a Microsoft developer account. An individual account is free when you start at <https://storedeveloper.microsoft.com> and choose **Individual developer**; Microsoft checks your identity with an ID and a selfie. Other sign-up routes still ask for a fee.
3. Go to <https://partner.microsoft.com/xboxconfig/devices>, enter the code, and select **Agree and activate**.
4. Back on the Xbox, in the Xbox Dev Mode app, select **Switch and restart**.

The console restarts into **Dev Home**. To go back to normal use later, open Dev Home and select **Leave Dev Mode**.

## 2. Turn on the Device Portal (once)

The Device Portal is the console's web page for installing apps and copying files.

1. On the Xbox, in Dev Home, on the **Home** tab, under **Remote Access**, select **Remote Access Settings**.
2. Check **Enable Xbox Device Portal**.
3. Under **Authentication**, select **Set username and password**, choose a user name and password, and **Save**.
4. **Close** the page. Note the address shown under **Remote Access**, such as `https://192.168.1.20:11443`.

On the computer, open that address in the browser. The browser warns that the connection is not private, because the console makes its own certificate: choose **Advanced** (or **Details**) and continue to the page. Sign in with the user name and password you just set. The page that opens is **Home**, with **My games & apps** at the top.

## 3. Install lba2cc

1. Download `lba2cc-<version>-xbox-x64.appx` from the [rolling pre-release](https://github.com/LBALab/lba2-classic-community/releases/tag/latest).
2. In the Device Portal, on **Home**, under **My games & apps**, select **Add**.
3. Select **Choose File**, pick the `.appx` you downloaded, and select **Next**.
4. The next page says **Choose any necessary dependencies**. lba2cc needs none: select **Start**.
5. Wait for **Package Successfully Registered**, then select **Done**. lba2cc is now in the list.

## 4. Start it once

In the list, open the **Actions** menu next to lba2cc and choose **Launch/Resume**, or start it from Dev Home on the Xbox. A user must be signed in on the Xbox, or it asks for one on the TV.

The game closes again straight away, without a message: it has no game files yet. That is expected. This first start makes the folder the game files go in.

## 5. Copy your game files

On the computer:

1. Find the folder that holds `LBA2.HQR`. In the 2022 Classic edition it is the `Common` folder inside the game's install folder. In Steam, right-click the game and choose **Manage** → **Browse local files**. In GOG Galaxy, open the game's settings menu and choose **Manage installation** → **Show folder**.
2. Copy that folder somewhere, for example to the desktop, and rename the copy `Game`, exactly.
3. Zip it. On Windows, right-click `Game` and choose **Compress to ZIP file** (on Windows 10: **Send to** → **Compressed (zipped) folder**). On a Mac, right-click `Game` and choose **Compress**. You get `Game.zip`, about 500 MB with the movies and voices.

In the Device Portal:

1. Select **File explorer** in the menu on the left.
2. Open **LocalAppData**, then **lba2cc**, then **LocalState**.
3. Under **Upload a file to this directory**, select **Choose File**, pick `Game.zip`, and select **Upload**.
4. The browser asks **Extract zip file contents to this directory?** Select **OK**.

Uploading takes a minute or two. When it is done, the page says `Game.zip uploaded.` and a `Game` folder is in the list.

A disc rip works too: put its `.bin` and `.cue` files in the `Game` folder instead.

## 6. Play

Start lba2cc again, as in step 4. It plays with an Xbox controller; [CONTROLLER.md](CONTROLLER.md) and the game's **Options** → **Controls** have the buttons. Quit from the game's menu.

## Updating

Download the newer `.appx` and install it as in step 3. Your game files, settings and saves are kept. If the game is running, the console closes it first.

## Backing up saves and settings

In **File explorer**, open **LocalAppData** → **lba2cc** → **LocalState**, then use the save icon in the **Save** column:

- next to `lba2.cfg` for your settings;
- next to `save` for your saves, which download as `save.zip`.

To put saves back, open the `save` folder, upload `save.zip`, and select **OK** when asked to extract it.

## Removing

On **Home**, open **Actions** next to lba2cc and choose **Uninstall**. It does not ask first, and it deletes the game files, settings and saves with the app, so back them up before.

## Known limits

- Played on an Xbox Series S only; an Xbox One has not been tried.
- No mouse or touch. Saves get automatic names, as the on-screen keyboard is not used.
- If the Xbox closes the game while it is in the background, progress since the last autosave is lost. Settings are kept.
- If something goes wrong, the game's log is `adeline.log` in **LocalState**: download it with the save icon and attach it when you [report the problem](https://github.com/LBALab/lba2-classic-community/issues).

How the build works, and what is still missing, is in [plan/XBOX_PLAN.md](plan/XBOX_PLAN.md).
