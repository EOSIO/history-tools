# Building the documentation

* Build and install https://github.com/foonathan/standardese
  * Point it at the CDT's llvm while building it
* Install `gitbook`:
  * `npm i -g gitbook-cli`
* Run the `generate-doc` script
  * If there's an error similar to the following, then rerun the script
    * `Error: ENOENT: no such file or directory, stat '.../_book/...'`
  * Result is in the `_book` directory
