# 2D LIDAR-Based Autonomous Mobile Robot Docking Station Detection

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

This repository provides a C application that processes 2D LIDAR scan data (in TOML format) to detect planar walls using sequential RANSAC and Principal Component Analysis (PCA), computes docking station corner apex coordinates ($\ge 60^\circ$ / $90^\circ$), and visualizes the robot's relative pose and range using Gnuplot.

---

## What the Program Does

1. **Input and Language Selection (`parseTomlScanFile`):** Offers interactive bilingual execution (English default, Turkish via selection) as well as scriptable CLI flags (`--tr`, `--en`). It reads local `.toml` files (with quote sanitization and wide-path support) or remote HTTP/HTTPS URLs using libcurl.
2. **Coordinate Transformation (`convertPolarToCartesian`):** Converts raw polar measurements ($r, \theta$) to 2D Cartesian coordinates ($x, y$) while discarding invalid readings (`NaN`, `Inf`, and out-of-range sensor returns via `filterInvalidLaserRanges`).
3. **Wall Detection (`extractWallsUsingRANSAC` + `fitLineUsingPCA`):** Extracts linear wall segments from the point cloud using sequential RANSAC. Line orientations and offsets are refined through Principal Component Analysis (PCA), preventing numerical singularities on vertical walls.
4. **Docking Corner Detection (`computeDockingIntersections`):** Computes pairwise angles between detected walls using vector dot products. Pairs meeting the angle threshold ($\ge 60^\circ$, typically $90^\circ$) are evaluated for physical intersection within wall segment bounds.
5. **Interactive Visualization (`renderVisualizationWithGnuplot`):** Spawns an auto-maximized Gnuplot window with an immediate white background, displaying the robot at $(0, 0)$, all scan points, extracted wall segments, docking apex coordinates, and distance vectors on a 1:1 isometric scale. The viewport supports pan (arrow keys), zoom (mouse wheel), and autoscale view reset (`a` key).

---

## Setup and Requirements

The program is written in standard C (C99).

### Prerequisites:
- **C Compiler:** MinGW GCC or Code::Blocks (bundled with MinGW)
- **libcurl:** Required for downloading remote TOML scan files (headers and dynamic libraries are supplied in the project)
- **Gnuplot:** Required for graphical visualization. It must be installed and added to the system `PATH` (executing `gnuplot` from a terminal launches Gnuplot).

### Building the Project:

**Method 1: Code::Blocks (Recommended)**
Open `Lidar/Proje.cbp` in Code::Blocks and press **F9** (Build and Run).

**Method 2: Command Line (MinGW GCC)**
From the `Lidar` directory:

```powershell
gcc -Wall -O2 -c main.c -o obj/Debug/main.o -I"C:/Program Files/curl-8.16.0_12-win64-mingw/include"
gcc -o bin/Debug/Proje.exe obj/Debug/main.o -L"C:/Program Files/curl-8.16.0_12-win64-mingw/lib" -lcurl -lm
```
*(Update `-I` and `-L` directory paths according to your local libcurl installation. The executable is generated under `bin/Debug/`, where `libcurl-x64.dll` resides).*

---

## Usage

When launched without CLI arguments, the program presents an interactive language selection prompt:

```text
================================================================================
        LIDAR RANSAC Line Detection and Docking / Şarj İstasyonu Tespiti        
================================================================================
Language / Dil:
  English  : [ENTER]
  Türkçe   : [2'ye bas + ENTER]
> 
```

After language selection, the file input prompt is displayed:

```text
Enter TOML file name, path, URL, or drag and drop: [ENTER = default]
> 
```

### Input Options:

1. **Press ENTER directly:** Uses the default laboratory benchmark URL (`http://abilgisayar.kocaeli.edu.tr/lidar1.toml`), downloads the payload, and executes the pipeline.
2. **Enter a file name:** E.g., `lidar.toml` or simply `lidar`. The application inspects the local directory as well as standard user directories (`Downloads`, `Desktop`, `Documents`, and `OneDrive`).
3. **Drag and Drop:** Drag any `.toml` file from File Explorer and release it into the console window. Enclosing quotation marks (`"..."` or `'...'`) are automatically stripped.
4. **Enter a URL:** Provide an explicit `http://` or `https://` endpoint pointing to a valid TOML scan file.
5. **Command-Line Arguments:** The interface can be bypassed directly from the terminal:
   ```powershell
   .\bin\Debug\Proje.exe --en samples/lidar.toml
   .\bin\Debug\Proje.exe --tr http://abilgisayar.kocaeli.edu.tr/lidar1.toml
   ```

### Sample Console Output:

```text
=== TOML Header ===
  Timestamp       : 1970-01-01T02:54:13
  Reference Frame : base_scan
  angle_min=0.0000  angle_max=6.2800  angle_increment=0.017493
  range_min=0.12  range_max=3.50  total measurements=360
====================
RANSAC extracted 8 linear segments.
Found 1 docking intersections with angle >= 60 deg:
Int 1 (d6 & d7): (-1.287, 1.369) dist=1.88m angle=89.8 deg
Press Enter to close plot... (Navigation: Arrow Keys = Pan, Ctrl+Wheel = Zoom, 'a' = 1:1 Reset)
```

### Interactive Visualization Navigation (Gnuplot)

When the visualization window opens, it is maximized automatically while preserving window focus and desktop switching. The Gnuplot `wxt` environment provides real-time keyboard and mouse controls:

| Input / Shortcut | Action | Description |
| :--- | :--- | :--- |
| **Arrow Keys** ($\leftarrow, \rightarrow, \uparrow, \downarrow$) | Viewport Pan | Translates the isometric viewing plane across the scan environment. |
| **Mouse Wheel / Trackpad Scroll** | Vertical Pan | Scrolls the viewport along the Y-axis. Horizontal trackpad scrolling delegates to the host OS driver settings. |
| **Ctrl + Mouse Wheel** | Cursor Zoom | Dynamically magnifies or demagnifies the point cloud centered at the mouse cursor. |
| **Right-Click Drag** | Box Zoom | Draws a rectangular bounding box to inspect local features (e.g. corner apex or wall inliers) in close-up detail. |
| **`1` and `2` Keys** | Coordinate Format | Cycles the bottom-left cursor readout between Cartesian $(x, y)$, Polar $(r, \theta)$, and auxiliary formats. |
| **`p` and `n` Keys** | Zoom History | Steps backward (`p`) and forward (`n`) through the viewport zoom history stack. |
| **`a` Key** | 1:1 Autoscale Reset | Restores the default 1:1 metric aspect ratio (`size ratio -1`), re-centering the robot, scan points, and docking walls. |

#### Toolbar and Operating System Interaction:
- **Top-Left Toolbar:** Houses quick actions for clipboard copy, image export, zoom history back/forward (magnifiers with blue and green arrows), and autoscale reset (magnifier with red plot curve). The metric grid is enabled by default (`#dddddd`) for coordinate readability.
- **Trackpad Scrolling Direction:** In Windows, two-finger horizontal trackpad gestures follow the system configuration (*Settings > Bluetooth & devices > Touchpad > Scrolling direction*).
- **Console Exit:** These shortcut hints are printed below the plot header and echoed in the terminal pause prompt. Pressing `Enter` in the console closes the visualization and terminates the process cleanly.

---

## Configurable Parameters

Algorithmic thresholds are configured in `main.c`:

| Parameter / Variable | Default Value | Location | Purpose | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `minAngleThresholdDegrees` | `60.0` deg | `main()` / `computeDockingIntersections` | Minimum corner angle for docking detection. | Project specification requires $\ge 60^\circ$. Lower values (e.g. $30^\circ$) risk matching non-docking corners. |
| `DISTANCE_TOLERANCE` | `0.025` m (2.5 cm) | `extractWallsUsingRANSAC` | Inlier distance threshold for RANSAC line fitting. | Accommodates LIDAR measurement noise. Values above 5 cm merge distinct parallel surfaces; values below 1 cm reject valid points. |
| `MIN_INLIER_THRESHOLD` | `8` points | `extractWallsUsingRANSAC` | Minimum support points required to validate a line. | Rejects transient noise and small clutter. |
| `MIN_WALL_LENGTH` | `0.22` m (22 cm) | `extractWallsUsingRANSAC` | Minimum physical length of an extracted wall segment. | Standard docking station wings span 25 to 30 cm. The 22 cm bound discards chair and table legs (10 to 15 cm). |
| `MAX_NEIGHBOR_GAP_SQ` | `0.35 * 0.35` (35 cm) | `extractWallsUsingRANSAC` | Maximum allowable Euclidean gap between adjacent inliers. | Prevents collinear walls separated by open doorways or corridors from merging into one segment. |
| `MAX_RANSAC_ITERATIONS` | `3000` | `extractWallsUsingRANSAC` | Random candidate hypothesis count per wall search. | For 360 points, 3000 trials execute in under 15 milliseconds and yield consistent convergence for orthogonal geometries. |
| Corner Tolerance | `0.25` m (25 cm) | `computeDockingIntersections` (`isPointNearLineSegment`) | Maximum allowable distance from intersection point to segment endpoints. | Enforces that line intersections correspond to physical corner junctions rather than unbounded geometric extrapolations. |

---

## Technical Architecture and Implementation

### 1. Algorithmic Pipeline

```
[ TOML / Web Input ]
         │
         ▼
[ parseTomlScanFile ]          ───>  Line-by-line streaming parser with dynamic array reallocation (realloc)
         │
         ▼
[ filterInvalidLaserRanges ]   ───>  Discards NaN / Inf and out-of-range sensor readings (-1)
         │
         ▼
[ convertPolarToCartesian ]    ───>  Maps (r, θ) to Cartesian (x, y) coordinates
         │
         ▼
[ extractWallsUsingRANSAC ]    ───>  Iteratively isolates consensus sets for dominant linear structures
         │
         ▼
[ fitLineUsingPCA ]            ───>  Minimizes orthogonal residuals via covariance eigenvectors (stable for vertical lines)
         │
         ▼
[ Gap Splitting & Clustering ] ───>  Sorts inliers along the wall axis and splits on spatial gaps > 35 cm
         │
         ▼
[ computeDockingIntersections ]───>  Solves line intersections and verifies corner proximity within 25 cm
         │
         ▼
[ renderVisualizationWithGnuplot ] ─> Pipes commands to Gnuplot with automatic full-screen and 1:1 isometric scaling
```

### 2. Algorithmic Design Choices

- **Corner Point Sharing in RANSAC:**  
  Standard RANSAC consumes inliers upon segment extraction. However, $90^\circ$ corner points belong to both intersecting walls. If one wall consumes the shared apex point, the second orthogonal wall can drop below the minimum inlier threshold. This implementation permits corner sharing between candidate lines when their mutual angle is at least $45^\circ$.
- **PCA Line Fitting:**  
  Ordinary Least Squares (OLS) regression degenerates when lines approach vertical orientation ($x = c$) due to unbounded slope ($m \to \infty$). Here, the orientation angle is calculated in closed form from the $2 \times 2$ covariance matrix: $\theta = \frac{1}{2} \text{atan2}(2C_{xy}, C_{xx} - C_{yy})$, ensuring numerical stability across all orientations.
- **Vector Dot Product for Angle Calculation:**  
  Angles between lines are evaluated using normalized direction vector dot products ($\cos\theta = \frac{|\vec{v}_1 \cdot \vec{v}_2|}{\|\vec{v}_1\| \|\vec{v}_2\|}$) rather than slope differences ($\frac{m_1 - m_2}{1 + m_1 m_2}$). This eliminates divide-by-zero singularities on orthogonal intersections.

---

### 3. Complexity Analysis

- **$N$:** Total scan points (typically 360).
- **$K$:** Extracted wall segments (typically 4 to 8).
- **$I$:** RANSAC iterations per extraction ($3000$).

#### Time Complexity:
1. **TOML Parsing:** $\mathcal{O}(N)$ (single pass over the stream).
2. **Coordinate Conversion:** $\mathcal{O}(N)$ (one trigonometric projection per point).
3. **RANSAC Wall Extraction:** $\mathcal{O}(K \cdot I \cdot N)$. For each wall, $I$ pairs are sampled and verified against $N$ points. For $N = 360$, $I = 3000$, and $K \approx 6$, total operations approximate $6 \times 10^6$ elementary floating-point operations, executing in **10 to 20 milliseconds** on commodity x86_64 hardware.
4. **Ordering and Clustering:** $\mathcal{O}(K \cdot M \log M)$, where $M \le N$ is the inlier count ($M \le 100$).
5. **Intersection Analysis:** $\mathcal{O}(K^2)$, performing pairwise checks across $K$ segments ($\binom{K}{2}$). For $K = 8$, this requires 28 operations ($\mathcal{O}(1)$ in practice).

$$\text{Total Time Complexity} = \mathcal{O}(K \cdot I \cdot N + K^2)$$
**Execution Time:** Local scan processing completes in **5 to 15 milliseconds**. Web download duration is bound by network latency.

#### Space Complexity:
- Points, inliers, and line models reside in contiguous dynamic buffers scaled to the point count.
- Peak heap usage remains **under 100 KB**.
$$\text{Total Space Complexity} = \mathcal{O}(N)$$

---

## Directory Structure

```
Proje1/
├── .gitignore                # Git exclusion list
├── README.md                 # English technical documentation
├── README.tr.md              # Turkish technical documentation
└── Lidar/
    ├── main.c                # Implementation (parser, geometry, RANSAC, visualization)
    ├── Proje.cbp             # Code::Blocks project file
    ├── samples/              # Sample LIDAR TOML scan datasets
    │   ├── lidar.toml
    │   ├── lidar2.toml
    │   └── scan_data_NaN.toml
    └── bin/
        └── Debug/
            ├── Proje.exe     # Compiled executable
            └── libcurl-x64.dll # Runtime library for network requests
```
