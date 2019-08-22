const path = require('path');
const CopyPlugin = require('copy-webpack-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const webpack = require('webpack');
const MonacoWebpackPlugin = require('monaco-editor-webpack-plugin');

module.exports = {
    entry: {
        client: ['./src-client/client.ts'],
    },
    module: {
        rules: [
            {
                test: /\.[tj]sx?$/,
                use: 'ts-loader',
                exclude: /node_modules/
            },
            {
                test: /\.css$/,
                use: [
                    { loader: "style-loader" },
                    { loader: "css-loader" }
                ]
            }
        ]
    },
    resolve: {
        extensions: ['.tsx', '.ts', '.js']
    },
    devtool: 'inline-source-map',
    devServer: {
        contentBase: './dist'
    },
    mode: 'development',
    plugins: [
        new HtmlWebpackPlugin({
            template: './src-client/index.html',
            filename: 'index.html'
        }),
        new webpack.NoEmitOnErrorsPlugin(),
        new CopyPlugin([
            { from: '../build/chain-client.wasm', to: 'chain-client.wasm' },
            { from: 'src-client/data-description.md', to: 'data-description.md' },
            { from: 'src-client/introduction.md', to: 'introduction.md' },
            { from: 'src-client/query-description.md', to: 'query-description.md' },
            { from: 'src/talk-client.cpp', to: 'talk-client.cpp' },
            { from: 'src/talk-client.wasm', to: 'talk-client.wasm' },
            { from: 'src/talk-server.cpp', to: 'talk-server.cpp' },
            { from: 'src/talk.cpp', to: 'talk.cpp' },
            { from: 'src/talk.hpp', to: 'talk.hpp' },
        ]),
        new MonacoWebpackPlugin({ languages: ['cpp', 'javascript', 'typescript'] }),
    ],
    output: {
        filename: '[name].bundle.js',
        path: path.resolve(__dirname, 'dist'),
        publicPath: '/'
    }
};
