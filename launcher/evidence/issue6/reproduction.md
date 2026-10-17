# Ticket #6 helper and preparation evidence

Reproduce from the repository root with:

```sh
launcher/run-issue6-acceptance.sh /tmp/pt-issue6-final
```

The 2026-10-17 run used the #6 branch after merging local `integration/launcher`
at `d1b06d6`, in the bound WSL2 worktree. The harness provisioned .NET SDK
10.0.402 for build/test only, cloned `https://github.com/maxton/LibOrbisPkg.git`
at pinned revision `643477263b2644e0803e0f58b8726ea4e3f3b7d4`, applied the
existing reader compatibility patch, and published self-contained `linux-x64`
and `win-x64` helpers. The staged trees include `LICENSE-LibOrbisPkg.txt` and
`LibOrbisPkg-corresponding-source.zip`.

Both actual staged helpers ran the synthetic US, Europe, Japan, and relocated
archive packages. The three extracted archive byte streams matched the
independent `installer/Tests/Program.cs` extraction for each case. Both helpers
also rejected malformed, unrelated-game, and retail/encrypted fixtures with
the expected messages and without creating output. The Windows helper ran
under native Windows PowerShell on Windows 11 build 26200. The Linux helper
ran in the WSL2 Linux environment recorded in `environment.txt`.

The Linux `PrepareGame` public-operation test copied the complete staged helper
directory into a disposable portable bundle and prepared from the synthetic
fPKG. It verified the source package bytes, player file, unowned file, extracted
archive recognition, and transaction cleanup. The full launcher suite passed
9/9 CTest checks. The separate real `std::filesystem::space` check used a
Docker-owned 64 MiB tmpfs (67,076,096 bytes available) and confirmed the public
preflight refuses preparation while preserving source and bundle files.

Helper SHA-256 values are in `helper-sha256.txt`. Build, publish, synthetic
generator, per-platform helper output, CTest, environment, and space logs are
saved alongside this note. All package contents are synthetic recognition
fixtures; this is not gameplay or proprietary-data evidence. Windows helper
execution is verified; native Windows launcher UI, SteamOS/Wayland target
compositors, and physical-device use are not covered by this ticket run.
