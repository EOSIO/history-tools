# Building the documentation

```
docker build -t history-tools-docs -f ./build-docs.dockerfile --build-arg BUILD_DATE=`date +%s` .
docker create --name history-tools-docs-temp history-tools-docs
docker cp history-tools-docs-temp:/root/history-tools/_book .
docker rm history-tools-docs-temp
```

The built documentation is at `/root/history-tools/_book` inside the image. This copies it to
the local `_book` directory.
