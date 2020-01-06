import * as React from 'react';
import * as ReactDOM from 'react-dom';
import Draggable from 'react-draggable';
import { FixedSizeList } from 'react-window';
import * as ql from './wasm-ql';

require('./style.css');

function prettyPrint(x: any) {
    return JSON.stringify(x, (k, v) => {
        if (k === 'abi' || k === 'account_abi') {
            if (v.length <= 66)
                return v;
            else
                return v.substr(0, 64) + '... (' + (v.length / 2) + ' bytes)';
        } else
            return v;
    }, 4);
}

class AppState {
    public alive = true;
    public clientRoot: ClientRoot;
    public result = [];
    public chainWasm = new ql.ClientWasm('./chain-client.wasm');
    public tokenWasm = new ql.ClientWasm('./token-client.wasm');
    public request = 0;
    public more = null;
    public queryInspector = false;
    public lastQuery = [];
    public replyInspector = false;
    public lastReply = {} as any;

    private run(wasm, query, args, firstKeyName, handle) {
        this.result = [];
        const thisRequest = ++this.request;
        let first_key = args[firstKeyName];
        let running = false;
        this.more = async () => {
            if (running)
                return;
            running = true;
            this.lastQuery = [
                query,
                { ...args, snapshot_block: ['absolute', args.snapshot_block], [firstKeyName]: first_key },
            ];
            const reply = await wasm.round_trip(this.lastQuery);
            if (thisRequest !== this.request)
                return;
            running = false;
            this.lastReply = reply[1];
            handle(reply[1]);
            first_key = reply[1].more;
            if (!first_key)
                this.more = null;
            this.clientRoot.forceUpdate();
        };
        this.clientRoot.forceUpdate();
    }

    public runSelected() {
        this.selection.run();
    }

    public accountsArgs = {
        snapshot_block: 100000000,
        first: '',
        last: '',
        include_abi: true,
        max_results: 10,
    };
    public run_accounts() {
        this.run(this.chainWasm, 'account', { ...this.accountsArgs, last: this.accountsArgs.last || 'zzzzzzzzzzzzj' }, 'first', reply => {
            for (let acc of reply.accounts) {
                acc = (({ name, privileged, account_creation_date, code, last_code_update, account_abi }) =>
                    ({ name, privileged, account_creation_date, code, last_code_update, abi: account_abi }))(acc);
                this.result.push(...prettyPrint(acc).split('\n'));
            }
        });
    }
    public accounts = { run: this.run_accounts.bind(this), form: AccountsForm };

    public multTokensArgs = {
        snapshot_block: 100000000,
        account: 'b1',
        first_key: { sym: '', code: '' },
        last_key: { sym: '', code: '' },
        max_results: 100,
    };
    public run_mult_tokens() {
        this.run(this.tokenWasm, 'bal.mult.tok', {
            ...this.multTokensArgs,
            last_key: { sym: this.multTokensArgs.last_key.sym || 'ZZZZZZZ', code: this.multTokensArgs.last_key.code || 'zzzzzzzzzzzzj' }
        }, 'first_key', reply => {
            for (const balance of reply.balances)
                this.result.push(balance.account.padEnd(13, ' ') + ql.format_extended_asset(balance.amount));
        });
    }
    public multipleTokens = { run: this.run_mult_tokens.bind(this), form: MultipleTokensForm };

    public tokensMultAccArgs = {
        snapshot_block: 100000000,
        code: 'eosio.token',
        sym: 'EOS',
        first_account: '',
        last_account: '',
        max_results: 10,
    };
    public run_tok_mul_acc() {
        this.run(this.tokenWasm, 'bal.mult.acc', { ...this.tokensMultAccArgs, last_account: this.tokensMultAccArgs.last_account || 'zzzzzzzzzzzzj' }, 'first_account', reply => {
            for (const balance of reply.balances)
                this.result.push(balance.account.padEnd(13, ' ') + ql.format_extended_asset(balance.amount));
        });
    }
    public tokensMultAcc = { run: this.run_tok_mul_acc.bind(this), form: TokensForMultipleAccountsForm };

    public transfersArgs = {
        snapshot_block: 100000000,
        first_key: {
            receiver: '',
            account: '',
            block: ['absolute', 0],
            transaction_id: '0000000000000000000000000000000000000000000000000000000000000000',
            action_ordinal: 0,
        },
        last_key: {
            receiver: '',
            account: 'zzzzzzzzzzzzj',
            block: ['absolute', 999999999],
            transaction_id: 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff',
            action_ordinal: 0xffffffff,
        },
        include_notify_incoming: true,
        include_notify_outgoing: true,
        max_results: 10,
    };
    public run_transfers() {
        this.run(this.tokenWasm, 'transfer', {
            ...this.transfersArgs,
            last_key: { ...this.transfersArgs.last_key, receiver: this.transfersArgs.last_key.receiver || 'zzzzzzzzzzzzj' }
        }, 'first_key', reply => {
            for (const transfer of reply.transfers)
                this.result.push(
                    transfer.from.padEnd(13, ' ') + ' -> ' + transfer.to.padEnd(13, ' ') +
                    ql.format_extended_asset(transfer.quantity).padEnd(40, ' ') +
                    (transfer.key.block[1] + '').padEnd(11, ' ') + transfer.key.transaction_id +
                    ' ' + transfer.memo);
        });
    }
    public transfers = { run: this.run_transfers.bind(this), form: TransfersForm };

    public selection = this.accounts;

    public restore(prev: AppState) {
        prev.request = -1;
        this.result = prev.result;
        this.accountsArgs = prev.accountsArgs;
        this.multTokensArgs = prev.multTokensArgs;
        this.tokensMultAccArgs = prev.tokensMultAccArgs;
        this.transfersArgs = prev.transfersArgs;
    }
}

async function delay(ms: number): Promise<void> {
    return new Promise((resolve, reject) => {
        setTimeout(resolve, ms);
    });
}

function Results({ appState }: { appState: AppState }) {
    const FSL = FixedSizeList as any;
    return (
        <div className='result'>
            <FSL
                itemCount={appState.result.length + (appState.more ? 10 : 0)}
                itemSize={16}
                height={800}
            >
                {({ index, style }) => {
                    let content = '';
                    if (index < appState.result.length)
                        content = appState.result[index];
                    else
                        if (appState.more) appState.more();
                    return <pre style={style}>{content}</pre>;
                }}
            </FSL>
            {appState.queryInspector &&
                <div className='query-inspect-container'>
                    <Draggable
                        axis='both'
                        handle='.handle'
                        defaultPosition={{ x: 10, y: 10 }}
                        position={null}
                        scale={1}
                    >
                        <div className='inspect'>
                            <div className='handle'>Query Inspector</div>
                            <pre className='inspect-content'>{prettyPrint(appState.lastQuery)}</pre>
                        </div>
                    </Draggable>
                </div>
            }
            {appState.replyInspector &&
                <div className='reply-inspect-container'>
                    <Draggable
                        axis='both'
                        handle='.handle'
                        defaultPosition={{ x: 10, y: 10 }}
                        position={null}
                        scale={1}
                    >
                        <div className='inspect'>
                            <div className='handle'>Reply Inspector</div>
                            {console.log(appState.lastReply)}
                            <pre className='inspect-content'>{prettyPrint({ more: appState.lastReply.more, ...appState.lastReply })}</pre>
                        </div>
                    </Draggable>
                </div>
            }
        </div>
    );
}

function AccountsForm({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    {/* <tr>
                        <td>snapshot_block  </td>
                        <td></td>
                        <td><input type="text" value={appState.accountsArgs.snapshot_block  } onChange={e => { appState.accountsArgs.snapshot_block    = +e.target.value; appState.runSelected(); }} /></td>
                    </tr> */}
                    <tr>
                        <td>min account</td>
                        <td></td>
                        <td><input type="text" value={appState.accountsArgs.first} onChange={e => { appState.accountsArgs.first = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>max account</td>
                        <td></td>
                        <td><input type="text" value={appState.accountsArgs.last} onChange={e => { appState.accountsArgs.last = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                </tbody>
            </table>
        </div>
    );
}

function MultipleTokensForm({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    {/* <tr>
                        <td>snapshot_block  </td>
                        <td></td>
                        <td><input type="text" value={appState.multTokensArgs.snapshot_block    } onChange={e => { appState.multTokensArgs.snapshot_block    = +e.target.value; appState.runSelected(); }} /></td>
                    </tr> */}
                    <tr>
                        <td>account</td>
                        <td></td>
                        <td><input type="text" value={appState.multTokensArgs.account} onChange={e => { appState.multTokensArgs.account = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    {/* <tr>
                        <td>first_key</td>
                        <td>sym</td>
                        <td><input type="text" value={appState.multTokensArgs.first_key.sym} onChange={e => { appState.multTokensArgs.first_key.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td></td>
                        <td>code</td>
                        <td><input type="text" value={appState.multTokensArgs.first_key.code} onChange={e => { appState.multTokensArgs.first_key.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>last_key</td>
                        <td>sym</td>
                        <td><input type="text" value={appState.multTokensArgs.last_key.sym} onChange={e => { appState.multTokensArgs.last_key.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td></td>
                        <td>code</td>
                        <td><input type="text" value={appState.multTokensArgs.last_key.code} onChange={e => { appState.multTokensArgs.last_key.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr> */}
                </tbody>
            </table>
        </div>
    );
}

function TokensForMultipleAccountsForm({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    {/* <tr>
                        <td>snapshot_block  </td>
                        <td></td>
                        <td><input type="text" value={appState.tokensMultAccArgs.snapshot_block } onChange={e => { appState.tokensMultAccArgs.snapshot_block  = +e.target.value; appState.runSelected(); }} /></td>
                    </tr> */}
                    <tr>
                        <td>code</td>
                        <td></td>
                        <td><input type="text" value={appState.tokensMultAccArgs.code} onChange={e => { appState.tokensMultAccArgs.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>sym</td>
                        <td></td>
                        <td><input type="text" value={appState.tokensMultAccArgs.sym} onChange={e => { appState.tokensMultAccArgs.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>min account</td>
                        <td></td>
                        <td><input type="text" value={appState.tokensMultAccArgs.first_account} onChange={e => { appState.tokensMultAccArgs.first_account = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>max account</td>
                        <td></td>
                        <td><input type="text" value={appState.tokensMultAccArgs.last_account} onChange={e => { appState.tokensMultAccArgs.last_account = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                </tbody>
            </table>
        </div>
    );
}

function TransfersForm({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    {/* <tr>
                        <td>snapshot_block  </td>
                        <td></td>
                        <td><input type="text" value={appState.transfersArgs.snapshot_block } onChange={e => { appState.transfersArgs.snapshot_block  = +e.target.value; appState.runSelected(); }} /></td>
                    </tr> */}
                    <tr>
                        <td>min receiver</td>
                        <td></td>
                        <td><input type="text" value={appState.transfersArgs.first_key.receiver} onChange={e => { appState.transfersArgs.first_key.receiver = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>max receiver</td>
                        <td></td>
                        <td><input type="text" value={appState.transfersArgs.last_key.receiver} onChange={e => { appState.transfersArgs.last_key.receiver = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>incoming</td>
                        <td></td>
                        <td><input type="checkbox" checked={appState.transfersArgs.include_notify_incoming} onChange={e => { appState.transfersArgs.include_notify_incoming = e.target.checked; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>outgoing</td>
                        <td></td>
                        <td><input type="checkbox" checked={appState.transfersArgs.include_notify_outgoing} onChange={e => { appState.transfersArgs.include_notify_outgoing = e.target.checked; appState.runSelected(); }} /></td>
                    </tr>
                </tbody>
            </table>
        </div>
    );
}

function Controls({ appState }: { appState: AppState }) {
    return (
        <div className='control'>
            <label>
                <input
                    type="radio"
                    checked={appState.selection === appState.accounts}
                    onChange={e => { appState.selection = appState.accounts; appState.runSelected(); }}>
                </input>
                Accounts
            </label>
            <label>
                <input
                    type="radio"
                    checked={appState.selection === appState.multipleTokens}
                    onChange={e => { appState.selection = appState.multipleTokens; appState.runSelected(); }}>
                </input>
                Multiple Tokens
            </label>
            <label>
                <input
                    type="radio"
                    checked={appState.selection === appState.tokensMultAcc}
                    onChange={e => { appState.selection = appState.tokensMultAcc; appState.runSelected(); }}>
                </input>
                Tokens For Mult Acc
            </label>
            <label>
                <input
                    type="radio"
                    checked={appState.selection === appState.transfers}
                    onChange={e => { appState.selection = appState.transfers; appState.runSelected(); }}>
                </input>
                Transfers
            </label>
            <br />
            <label>
                <input
                    type='checkbox'
                    checked={appState.queryInspector}
                    onChange={e => { appState.queryInspector = e.target.checked; appState.clientRoot.forceUpdate(); }}>
                </input>
                Query Inspector
            </label>
            <label>
                <input
                    type='checkbox'
                    checked={appState.replyInspector}
                    onChange={e => { appState.replyInspector = e.target.checked; appState.clientRoot.forceUpdate(); }}>
                </input>
                Reply Inspector
            </label>
        </div>
    );
}

class ClientRoot extends React.Component<{ appState: AppState }> {
    public render() {
        const { appState } = this.props;
        appState.clientRoot = this;
        return (
            <div className='client-root'>
                <div className='banner'>
                    Example application demonstrating wasm-ql
                </div>
                <Controls appState={appState} />
                {appState.selection.form({ appState })}
                <Results appState={appState} />
                <div className='disclaimer'>
                    <a href="https://github.com/EOSIO/history-tools">GitHub Repo...</a>
                    <br /><br />
                    Disclaimer: All repositories and other materials are provided subject to this
                    &nbsp;<a href="https://block.one/important-notice/">IMPORTANT</a>&nbsp;
                    notice and you must familiarize yourself with its terms.  The notice contains
                    important information, limitations and restrictions relating to our software,
                    publications, trademarks, third-party resources, and forward-looking statements.
                    By accessing any of our repositories and other materials, you accept and agree
                    to the terms of the notice.
                </div>
            </div>
        );
    }
}

export default function init(prev: AppState) {
    let appState = new AppState();
    if (prev) {
        appState.restore(prev);
        prev.alive = false;
    }
    ReactDOM.render(<ClientRoot {...{ appState }} />, document.getElementById('main'));
    appState.runSelected();
    return appState;
}
