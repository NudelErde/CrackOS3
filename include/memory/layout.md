# Memory layout:
| From   | To     | Size  | Description                   |
|--------|--------|-------|-------------------------------|
| 0 Tib  | 16Tib  | 16Tib | Program Space                 |
| 16Tib  | 32Tib  | 16Tib | Method argument mapping space |
| 32Tib  | 64Tib  | 32Tib | Kernel                        |
| 64Tib  | 96Tib  | 32Tib | Identity Map                  |
| 96Tib  | 126Tib | 30Tib | Kernel Heap                   |
| 126Tib | 128Tib | 2Tib  | PCI-NonPref                   |
