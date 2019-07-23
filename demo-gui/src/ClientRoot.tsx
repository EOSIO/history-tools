import * as React from 'react';
import * as ReactDOM from 'react-dom';
import * as ql from './wasm-ql';

require('./style.css');

class AppState {
    public alive = true;
    public clientRoot: ClientRoot;
    public result = '';
    public chainWasm = new ql.ClientWasm('./chain-client.wasm');
    public tokenWasm = new ql.ClientWasm('./token-client.wasm');
    public request = 0;

    private async run(query, args, firstKeyName, handle) {
        this.result = '';
        this.clientRoot.forceUpdate();
        let thisRequest = ++this.request;
        let first_key = args[firstKeyName];
        do {
            const reply = await this.tokenWasm.round_trip([
                query,
                { ...args, max_block: ['head', args.max_block], [firstKeyName]: first_key }
            ]);
            if (thisRequest !== this.request)
                return;
            handle(reply[1]);
            first_key = reply[1].more;
            break; // todo
        } while (first_key);
        this.clientRoot.forceUpdate();
    }

    public async runSelected() {
        this.selection.run();
    }

    public mult_tokens = {
        max_block: 50000000,
        account: 'b1',
        first_key: { sym: '', code: '' },
        last_key: { sym: 'ZZZZZZZ', code: 'zzzzzzzzzzzzj' },
        max_results: 1000,
    };
    public async run_mult_tokens() {
        await this.run('bal.mult.tok', this.mult_tokens, 'first_key', reply => {
            for (const balance of reply.balances)
                this.result += balance.account.padEnd(13, ' ') + ql.format_extended_asset(balance.amount) + '\n';
        });
    }
    public multipleTokens = { run: this.run_mult_tokens.bind(this), form: MultipleTokens };

    public tok_mul_acc = {
        max_block: 50000000,
        code: 'eosio.token',
        sym: 'EOS',
        first_account: '',
        last_account: 'zzzzzzzzzzzzj',
        max_results: 1000,
    };
    public async run_tok_mul_acc() {
        await this.run('bal.mult.acc', this.tok_mul_acc, 'first_account', reply => {
            for (const balance of reply.balances)
                this.result += balance.account.padEnd(13, ' ') + ql.format_extended_asset(balance.amount) + '\n';
        });
    }
    public tokensMultAcc = { run: this.run_tok_mul_acc.bind(this), form: TokensForMultipleAccounts };

    public selection = this.multipleTokens;

    public restore(prev: AppState) {
        this.result = prev.result;
        this.mult_tokens = prev.mult_tokens;
    }
}

function appendMessage(appState: AppState, result: string) {
    appState.result += result + '\n';
    appState.clientRoot.forceUpdate();
}

async function delay(ms: number): Promise<void> {
    return new Promise((resolve, reject) => {
        setTimeout(resolve, ms);
    });
}

function MultipleTokens({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    <tr>
                        <td>max_block</td>
                        <td></td>
                        <td><input type="text" value={appState.mult_tokens.max_block} onChange={e => { appState.mult_tokens.max_block = +e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>account</td>
                        <td></td>
                        <td><input type="text" value={appState.mult_tokens.account} onChange={e => { appState.mult_tokens.account = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>first_key</td>
                        <td>sym</td>
                        <td><input type="text" value={appState.mult_tokens.first_key.sym} onChange={e => { appState.mult_tokens.first_key.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td></td>
                        <td>code</td>
                        <td><input type="text" value={appState.mult_tokens.first_key.code} onChange={e => { appState.mult_tokens.first_key.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>last_key</td>
                        <td>sym</td>
                        <td><input type="text" value={appState.mult_tokens.last_key.sym} onChange={e => { appState.mult_tokens.last_key.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td></td>
                        <td>code</td>
                        <td><input type="text" value={appState.mult_tokens.last_key.code} onChange={e => { appState.mult_tokens.last_key.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                </tbody>
            </table>
        </div>
    );
}

function TokensForMultipleAccounts({ appState }: { appState: AppState }) {
    return (
        <div className='balance'>
            <table>
                <tbody>
                    <tr>
                        <td>max_block</td>
                        <td></td>
                        <td><input type="text" value={appState.tok_mul_acc.max_block} onChange={e => { appState.tok_mul_acc.max_block = +e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>code</td>
                        <td></td>
                        <td><input type="text" value={appState.tok_mul_acc.code} onChange={e => { appState.tok_mul_acc.code = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>sym</td>
                        <td></td>
                        <td><input type="text" value={appState.tok_mul_acc.sym} onChange={e => { appState.tok_mul_acc.sym = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>first_account</td>
                        <td></td>
                        <td><input type="text" value={appState.tok_mul_acc.first_account} onChange={e => { appState.tok_mul_acc.first_account = e.target.value; appState.runSelected(); }} /></td>
                    </tr>
                    <tr>
                        <td>last_account</td>
                        <td></td>
                        <td><input type="text" value={appState.tok_mul_acc.last_account} onChange={e => { appState.tok_mul_acc.last_account = e.target.value; appState.runSelected(); }} /></td>
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
                <pre className='result'>{appState.result}</pre>
                <div className='disclaimer'>
                    <a href="https://github.com/EOSIO/history-tools">GitHub Repo</a>
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
    return appState;
}
