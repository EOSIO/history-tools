import * as React from 'react';
import * as ReactDOM from 'react-dom';
import Draggable from 'react-draggable';
import ReactMarkdown from 'react-markdown';
import MonacoEditor from 'react-monaco-editor';
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
    public talkWasm = new ql.ClientWasm('./talk-client.wasm');
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
                { ...args, snapshot_block: args.snapshot_block === undefined ? undefined : ['absolute', args.snapshot_block], [firstKeyName]: first_key },
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
        if (this.selection.run)
            this.selection.run();
        else
            this.clientRoot.forceUpdate();
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

    public messageCache = new Map<string, any>();
    public messagesArgs = {
        begin: {
            parent_ids: [],
            id: 0,
        },
        max_messages: 10,
    };
    public run_messages() {
        this.run(this.talkWasm, 'get.messages', this.messagesArgs, 'begin', reply => {
            for (const m of reply.messages) {
                const msg = { ...m };
                if (msg.reply_to === '0')
                    msg.depth = 0;
                else
                    msg.depth = this.messageCache.get(msg.reply_to).depth + 1;
                this.messageCache.set(msg.id, msg);
                this.result.push((' '.repeat(msg.depth * 4) + msg.user.padEnd(13)).padEnd(80) + ' ' + msg.content);
            }
        });
    }
    public messages = { run: this.run_messages.bind(this), form: MessagesForm };

    public introduction = { title: 'Introduction', filename: 'introduction.md', run: null, form: MarkdownForm };
    public dataDesc = { title: 'Data Description', filename: 'data-description.md', run: null, form: MarkdownForm };
    public queryDesc = { title: 'Query Description', filename: 'query-description.md', run: null, form: MarkdownForm };
    public talkHpp = { title: 'talk.hpp', filename: 'talk.hpp', run: null, form: SourceForm };
    public talkCpp = { title: 'talk.cpp (contract)', filename: 'talk.cpp', run: null, form: SourceForm };
    public talkServer = { title: 'talk-server.cpp', filename: 'talk-server.cpp', run: null, form: SourceForm };
    public talkClient = { title: 'talk-client.cpp', filename: 'talk-client.cpp', run: null, form: SourceForm };
    public fill = { title: 'fill.js', filename: 'fill.js', run: null, form: SourceForm };

    public selection = this.introduction as any;

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
        </div>
    );
}

function MessagesForm({ appState }: { appState: AppState }) {
    return (
        <div>
            <ul>
                <li>This fetches messages from the tree as needed; scroll the area below to see more.</li>
                <li>A process running in the background pushes transactions containing new messages.
                    &nbsp;<a href='#' onClick={e => appState.runSelected()}>Click this to refresh.</a></li>
                <li>Use the Query Inspector and Reply Inspector (left side of page) to see the query and
                    reply during scrolling.</li>

            </ul>
        </div >
    );
}
function AccountsForm({ appState }: { appState: AppState }) {
    return (
        <div>
            Accounts on this chain:
            <br /><br />
        </div>
    );
}

function MarkdownForm({ appState }: { appState: AppState }) {
    const sel = appState.selection as any;
    if (!sel.loading) {
        sel.loading = true;
        sel.content = '';
        (async () => {
            const resp = await fetch(sel.filename);
            sel.content = await resp.text();
            if (appState.selection === sel)
                appState.clientRoot.forceUpdate();
        })();
    }
    return (
        <div className='markdown'>
            <ReactMarkdown source={sel.content} />
        </div>
    );
}

function SourceForm({ appState }: { appState: AppState }) {
    const sel = appState.selection as any;
    if (!sel.loading) {
        sel.loading = true;
        (async () => {
            const resp = await fetch(sel.filename);
            sel.content = await resp.text();
            if (appState.selection === sel)
                appState.clientRoot.forceUpdate();
        })();
    }
    if (sel.content)
        return (
            <MonacoEditor
                width="800"
                height="800"
                language="cpp"
                theme="vs-dark"
                value={sel.content}
            />
        );
    else
        return <div />;
}

function ContentRadio({ appState, selection }) {
    return (
        <label>
            <input
                type='radio'
                checked={appState.selection === selection}
                onChange={e => { appState.selection = selection; appState.runSelected(); }}>
            </input>
            {selection.title}
        </label>
    );
}

function Controls({ appState }: { appState: AppState }) {
    return (
        <div className='control'>
            <ContentRadio {...{ appState, selection: appState.introduction }} />
            <br />
            <label>
                <input
                    type='radio'
                    checked={appState.selection === appState.messages}
                    onChange={e => { appState.selection = appState.messages; appState.runSelected(); }}>
                </input>
                Messages Query
            </label>
            <label>
                <input
                    type='radio'
                    checked={appState.selection === appState.accounts}
                    onChange={e => { appState.selection = appState.accounts; appState.runSelected(); }}>
                </input>
                Accounts Query
            </label>
            <br />
            <ContentRadio {...{ appState, selection: appState.dataDesc }} />
            <ContentRadio {...{ appState, selection: appState.queryDesc }} />
            <br />
            <ContentRadio {...{ appState, selection: appState.talkHpp }} />
            <ContentRadio {...{ appState, selection: appState.talkCpp }} />
            <ContentRadio {...{ appState, selection: appState.talkServer }} />
            <ContentRadio {...{ appState, selection: appState.talkClient }} />
            <ContentRadio {...{ appState, selection: appState.fill }} />
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
                <div className='center'>
                    {appState.selection.form({ appState })}
                    {appState.selection.run && <Results appState={appState} />}
                </div>
                <div className='disclaimer'>
                    <a href="https://github.com/EOSIO/history-tools">GitHub Repo...</a>
                </div>
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
                                <pre className='inspect-content'>{prettyPrint({ more: appState.lastReply.more, ...appState.lastReply })}</pre>
                            </div>
                        </Draggable>
                    </div>
                }
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
