rm app.icns
mkdir app.iconset
sips -z 16 16     app.png --out app.iconset/icon_16x16.png
sips -z 32 32     app.png --out app.iconset/icon_16x16@2x.png
sips -z 32 32     app.png --out app.iconset/icon_32x32.png
sips -z 64 64     app.png --out app.iconset/icon_32x32@2x.png
sips -z 128 128   app.png --out app.iconset/icon_128x128.png
sips -z 256 256   app.png --out app.iconset/icon_128x128@2x.png
sips -z 256 256   app.png --out app.iconset/icon_256x256.png
sips -z 512 512   app.png --out app.iconset/icon_256x256@2x.png
sips -z 512 512   app.png --out app.iconset/icon_512x512.png
cp app.png app.iconset/icon_512x512@2x.png
iconutil -c icns app.iconset
rm -R app.iconset