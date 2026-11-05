# Wireless updates on the Kindle

The existing LUMEN Connect address can be the upload point for future updates.
The browser already uploads files over the local network, and the device
launcher already copies the executable into RAM before starting it. This gives
the next launch a natural point to activate an update. The current installer is
a host-side Python tool and expects a mounted Kindle filesystem.

A practical first version would use the same Connect page and this flow:

1. The user pairs with the Kindle and uploads a LUMEN update bundle. Connect
   stages it separately from the active installation and personal data.
2. A device-side verifier checks the bundle's release signature, version,
   hardware target, paths, free space and file hashes. The release signing
   public key is installed with the updater. A checksum from an uploaded
   manifest alone does not authenticate who built the package.
3. The Kindle shows the verified version and asks the user to confirm.
   LUMEN saves its state and exits normally.
4. The launcher activates the staged runtime on the next start. It retains the
   previous runtime and selects it again if the new version fails its startup
   health check. The updater and recovery entry point stay outside the files
   being swapped. Books, notes, photos and settings stay outside both slots.

For the first version, the user can download the release on a phone or
computer, then upload it over the LAN. An automatic release check can use the
same signed bundle format later. The current pairing PIN controls access to
Connect. It is separate from verifying a release signature.

An existing Kindle needs the updater and recovery launcher installed once
before it can receive wireless releases. Version 2.6 contains the photo and
appearance changes and this design. Its installer still uses USB. Device-side
verification, staged activation and rollback need implementation and testing
on a Kindle before they can replace the current installation path.
