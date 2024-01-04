# ImageView

Minimalist Image Viewer. Yes, I know there are a million of these already, none of them are quite what I want.

### Goals

- Minimize application size and system resource usage
- Minimalist, responsive UI
- Flexible user input settings
- Handle most common image file formats
- Run standalone with no installer
- Rotate/flip images
- Can be localized without too much hassle

### Limitations

- Windows only (Windows 7 and newer)
- Cannot edit or mark up images

### Implementation

- Native Win32 APIs
- C++ std library
- WIC for all image load/save operations
- UTF16 exclusively
- D3D11 for drawing
- DirectWrite for text output
- Registry to store settings
