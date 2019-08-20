const path = require('path');
const CopyPlugin = require('copy-webpack-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const webpack = require('webpack');

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
            { from: 'src/talk-client.wasm', to: 'talk-client.wasm' },
        ]),
    ],
    output: {
        filename: '[name].bundle.js',
        path: path.resolve(__dirname, 'dist'),
        publicPath: '/'
    }
};
