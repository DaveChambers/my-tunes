# TuneList macOS Release / Signing README

This is the simple, repeatable release guide for the **TuneList** standalone macOS app.

TuneList is simpler than the MES synth/plugin releases because it is just a `.app`, not AU/VST plugin binaries plus installer packages.

The release pattern is:

```text
sign app -> verify signing -> zip for Apple -> notarize -> staple -> validate -> final user ZIP
```

---

## 0. Important paths

The real TuneList project is here:

```bash
/Users/davechambers/Code/MES/my-tunes
```

Do **not** use this old/empty mistaken folder:

```bash
/Users/davechambers/Code/my-tunes
```

The built app should be here:

```bash
/Users/davechambers/Code/MES/my-tunes/build/TuneList_artefacts/TuneList.app
```

The final release ZIP will be created here:

```bash
/Users/davechambers/Code/MES/my-tunes/Releases/0.1.0/TuneList-0.1.0-macOS.zip
```

---

## 1. Go to the project folder

```bash
cd /Users/davechambers/Code/MES/my-tunes
```

Check you are in the right place:

```bash
pwd
ls -la
```

You should see files/folders like:

```text
CMakeLists.txt
Source
Assets
TuneList.jucer
README.md
build
```

---

## 2. Set release variables

Run this from:

```bash
/Users/davechambers/Code/MES/my-tunes
```

```bash
export TUNELIST_PROJECT_DIR="$PWD"
export TUNELIST_APP_PATH="$TUNELIST_PROJECT_DIR/build/TuneList_artefacts/TuneList.app"
export TUNELIST_VERSION="0.1.0"
export TUNELIST_RELEASE_DIR="$TUNELIST_PROJECT_DIR/Releases/$TUNELIST_VERSION"
export TUNELIST_APP_CERT='Developer ID Application: Make Electronic Sound OU'
export TUNELIST_NOTARY_PROFILE="NOTARYTOOL_PASSWORD"
```

Check the app exists:

```bash
ls -ld "$TUNELIST_APP_PATH"
```

Good output should point at:

```text
/Users/davechambers/Code/MES/my-tunes/build/TuneList_artefacts/TuneList.app
```

---

## 3. Check the signing certificate exists

```bash
security find-identity -v -p codesigning | grep "Developer ID Application: Make Electronic Sound OU"
```

Good output looks like:

```text
Developer ID Application: Make Electronic Sound OU (VYL8JBMP9C)
```

The Team ID is:

```text
VYL8JBMP9C
```

---

## 4. Create the release folder

```bash
mkdir -p "$TUNELIST_RELEASE_DIR"
```

---

## 5. Clear old Mac attributes from the app

```bash
xattr -cr "$TUNELIST_APP_PATH"
```

---

## 6. Sign the app

```bash
codesign --force --deep --options runtime --timestamp \
  --sign "$TUNELIST_APP_CERT" \
  "$TUNELIST_APP_PATH"
```

It may say:

```text
replacing existing signature
```

That is fine.

---

## 7. Verify the app signature

```bash
codesign --verify --deep --strict --verbose=2 "$TUNELIST_APP_PATH"
```

Good output:

```text
valid on disk
satisfies its Designated Requirement
```

---

## 8. Make the notarization ZIP

This ZIP is for Apple only. It is **not** the final user download.

```bash
export TUNELIST_NOTARY_ZIP="$TUNELIST_RELEASE_DIR/TuneList-$TUNELIST_VERSION-notary.zip"

rm -f "$TUNELIST_NOTARY_ZIP"

ditto -c -k --keepParent \
  "$TUNELIST_APP_PATH" \
  "$TUNELIST_NOTARY_ZIP"

ls -lh "$TUNELIST_NOTARY_ZIP"
```

Expected result:

```text
TuneList-0.1.0-notary.zip
```

---

## 9. Store notarization credentials, only if needed

You only need this step if the saved notarization profile is missing, expired, or wrong.

```bash
xcrun notarytool store-credentials "$TUNELIST_NOTARY_PROFILE" \
  --apple-id "dave@makeelectronicsound.com" \
  --team-id "VYL8JBMP9C"
```

When asked for the password, paste an **Apple app-specific password**, not the normal Apple password.

Good output:

```text
Success. Credentials validated.
Credentials saved to Keychain.
```

If you get:

```text
401 Invalid credentials
```

then the Apple ID or app-specific password is wrong.

If you get:

```text
403 required agreement is missing or expired
```

then check Apple Developer / App Store Connect agreements.

---

## 10. Submit to Apple notarization

```bash
xcrun notarytool submit "$TUNELIST_NOTARY_ZIP" \
  --keychain-profile "$TUNELIST_NOTARY_PROFILE" \
  --wait
```

Wait until Apple finishes.

Good final output should say:

```text
Accepted
```

Do **not** continue until it says **Accepted**.

---

## 11. Staple the notarization ticket to the app

```bash
xcrun stapler staple "$TUNELIST_APP_PATH"
```

Good output should say something like:

```text
The staple and validate action worked!
```

---

## 12. Validate the staple

```bash
xcrun stapler validate "$TUNELIST_APP_PATH"
```

Good output means the notarization ticket is attached properly.

---

## 13. Gatekeeper check

```bash
spctl --assess --type execute --verbose=4 "$TUNELIST_APP_PATH"
```

Good output should say something like:

```text
accepted
```

---

## 14. Make the final user ZIP

This is the file to upload to the website/cloud storage.

```bash
export TUNELIST_FINAL_ZIP="$TUNELIST_RELEASE_DIR/TuneList-$TUNELIST_VERSION-macOS.zip"

rm -f "$TUNELIST_FINAL_ZIP"

ditto -c -k --keepParent \
  "$TUNELIST_APP_PATH" \
  "$TUNELIST_FINAL_ZIP"

ls -lh "$TUNELIST_FINAL_ZIP"
```

Final upload file:

```bash
/Users/davechambers/Code/MES/my-tunes/Releases/0.1.0/TuneList-0.1.0-macOS.zip
```

Upload this one:

```text
TuneList-0.1.0-macOS.zip
```

Do **not** upload this one:

```text
TuneList-0.1.0-notary.zip
```

The `-notary.zip` file was only for Apple.

---

## 15. Final release checklist

Before putting the link on the website:

```text
[ ] App is signed
[ ] codesign verify passes
[ ] Apple notarization says Accepted
[ ] stapler staple succeeds
[ ] stapler validate succeeds
[ ] spctl says accepted
[ ] Final ZIP exists
[ ] Final ZIP is called TuneList-0.1.0-macOS.zip
```

Then upload:

```text
TuneList-0.1.0-macOS.zip
```

---

## 16. Short version for future Dave

Use this once everything is set up and you just need to release another build.

```bash
cd /Users/davechambers/Code/MES/my-tunes

export TUNELIST_PROJECT_DIR="$PWD"
export TUNELIST_APP_PATH="$TUNELIST_PROJECT_DIR/build/TuneList_artefacts/TuneList.app"
export TUNELIST_VERSION="0.1.0"
export TUNELIST_RELEASE_DIR="$TUNELIST_PROJECT_DIR/Releases/$TUNELIST_VERSION"
export TUNELIST_APP_CERT='Developer ID Application: Make Electronic Sound OU'
export TUNELIST_NOTARY_PROFILE="NOTARYTOOL_PASSWORD"

mkdir -p "$TUNELIST_RELEASE_DIR"

xattr -cr "$TUNELIST_APP_PATH"

codesign --force --deep --options runtime --timestamp \
  --sign "$TUNELIST_APP_CERT" \
  "$TUNELIST_APP_PATH"

codesign --verify --deep --strict --verbose=2 "$TUNELIST_APP_PATH"

export TUNELIST_NOTARY_ZIP="$TUNELIST_RELEASE_DIR/TuneList-$TUNELIST_VERSION-notary.zip"

rm -f "$TUNELIST_NOTARY_ZIP"

ditto -c -k --keepParent \
  "$TUNELIST_APP_PATH" \
  "$TUNELIST_NOTARY_ZIP"

xcrun notarytool submit "$TUNELIST_NOTARY_ZIP" \
  --keychain-profile "$TUNELIST_NOTARY_PROFILE" \
  --wait

xcrun stapler staple "$TUNELIST_APP_PATH"

xcrun stapler validate "$TUNELIST_APP_PATH"

spctl --assess --type execute --verbose=4 "$TUNELIST_APP_PATH"

export TUNELIST_FINAL_ZIP="$TUNELIST_RELEASE_DIR/TuneList-$TUNELIST_VERSION-macOS.zip"

rm -f "$TUNELIST_FINAL_ZIP"

ditto -c -k --keepParent \
  "$TUNELIST_APP_PATH" \
  "$TUNELIST_FINAL_ZIP"

echo "Final upload file:"
echo "$TUNELIST_FINAL_ZIP"
```

---

## 17. Add this README to source control

Copy this file into the project, for example:

```bash
cp ~/Downloads/TuneList_RELEASE_SIGNING_README.md /Users/davechambers/Code/MES/my-tunes/RELEASE_SIGNING_README.md
```

Then commit it:

```bash
cd /Users/davechambers/Code/MES/my-tunes

git status
git add RELEASE_SIGNING_README.md
git commit -m "Add TuneList release signing README"
git push
```
