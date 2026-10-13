set title "LIDAR Doğruları ve Kesişim Noktaları"
set xlabel "X (metre)"
set ylabel "Y (metre)"
set grid
set size square
set key outside top right
set pointsize 0.8

# Robot konumu
set object 1 circle at 0,0 size 0.05 fc rgb "red" fs solid border lc rgb "black"
set label 1 "Robot (0,0)" at 0.1,0.1 tc rgb "red"

# Çizim
plot \
    "lidar_points.dat" using 1:2 with points pt 7 ps 0.4 lc rgb "blue" title "Lidar Noktaları", \
    "pca_line.dat" using 1:2 with lines lw 2 lc rgb "orange" title "Doğrular", \
    "intersections.dat" using 1:2 with points pt 13 ps 1.2 lc rgb "green" title "Kesişim Noktaları", \
    "intersections.dat" using 1:2:3 with labels offset 0.15,0.15 font ",8" tc rgb "black" notitle
pause -1 "Kapatmak için Enter’a bas..."
