# EOSIO History Tools

The history tools repo has these components:

* Database fillers connect to the nodeos state-history plugin and populate databases
* wasm-ql servers answer incoming queries by running server WASMs, which have read-only access to the databases
* The wasm-ql library, when combined with the CDT library, provides utilities that server WASMs and client WASMs need
* A set of example server WASMs and client WASMs

| App             | Fills LMDB | wasm-ql with LMDB          | Fills PostgreSQL | wasm-ql with PostgreSQL |
| --------------- | ---------- | -------------------------- | ---------------- | ---------------- |
| `fill-lmdb`     | Yes        |                            |                  |                  |        
| `wasm-ql-lmdb`  |            | Yes                        |                  |                  |            
| `combo-lmdb`    | Yes        | Yes                        |                  |                  |            
| `fill-pg`       |            |                            | Yes              |                  |        
| `wasm-ql-pg`    |            |                            |                  | Yes              |            
| `history-tools` | Yes*       | Yes*                       | Yes*             | Yes*             |            

Note: by default, `history-tools` does nothing; use the `--plugin` option to select plugins.

See the [documentation site](https://eosio.github.io/wasm-api/)

## Contributing

[Contributing Guide](./CONTRIBUTING.md)

[Code of Conduct](./CONTRIBUTING.md#conduct)

## License

[MIT](./LICENSE)

## Important

See LICENSE for copyright and license terms.  Block.one makes its contribution on a voluntary basis as a member of the EOSIO community and is not responsible for ensuring the overall performance of the software or any related applications.  We make no representation, warranty, guarantee or undertaking in respect of the software or any related documentation, whether expressed or implied, including but not limited to the warranties or merchantability, fitness for a particular purpose and noninfringement. In no event shall we be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the software or documentation or the use or other dealings in the software or documentation.  Any test results or performance figures are indicative and will not reflect performance under all conditions.  Any reference to any third party or third-party product, service or other resource is not an endorsement or recommendation by Block.one.  We are not responsible, and disclaim any and all responsibility and liability, for your use of or reliance on any of these resources. Third-party resources may be updated, changed or terminated at any time, so the information here may be out of date or inaccurate.
