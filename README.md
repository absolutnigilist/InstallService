# InstallService

Cross-platform service installer skeleton:
- Windows: backend stub for sc.exe
- Linux: backend stub for systemd

Build:
cmake -S . -B out/build
cmake --build out/build --config Debug

Examples:
  service-installer --install --name=Valenta --exe=Valenta.exe --run
  service-installer --uninstall --name=Valenta --stop
  service-installer --start --name=Valenta
  service-installer --stop  --name=Valentaïî
  ServiceInstaller.exe --uninstall --name=Valenta --stop-first --from-inno --delete=data --data-root=...
