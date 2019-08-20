import * as React from 'react';
import * as ReactDOM from 'react-dom';
import Draggable from 'react-draggable';
import { FixedSizeList } from 'react-window';
import * as ql from '../../demo-gui/src/wasm-ql';

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


    public selection = this.accounts;

    public restore(prev: AppState) {
        prev.request = -1;
        this.result = prev.result;
        this.accountsArgs = prev.accountsArgs;
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
