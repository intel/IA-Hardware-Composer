## Build Color Formatter
```
cd path/to/colorformatter/

./autogen.sh

make
```

## Generate Test Image
The script will use ffmpeg to convert image media file to the original raw data which can be used by colorformatter
to convert the final test image files.

```
sudo apt-get install ffmpeg

sh resources/commands.sh
```
