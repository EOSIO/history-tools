// copyright defined in LICENSE.txt

struct permission_level_weight {
    eosio::permission_level permission;
    uint16_t                weight;
};

struct key_weight {
    eosio::public_key key;
    uint16_t          weight;
};

struct wait_weight {
    uint32_t wait_sec;
    uint16_t weight;
};

struct authority {
    uint32_t                             threshold = 0;
    std::vector<key_weight>              keys;
    std::vector<permission_level_weight> accounts;
    std::vector<wait_weight>             waits;
};

struct updateauth {
    name      account;
    name      permission;
    name      parent;
    authority auth;
};

html to_html(const key_weight& kw) {
    html result = R"(
        <tr><td>Key</td><td>{key}</td><td>{weight}</td></tr>
    )";

    result = result.replace("{key}", kw.key);
    result = result.replace("{weight}", kw.weight);
    return result;
}

html to_html(const updateauth& action_data) {
    html result = R"(
        Modify permission <b>{permission}</b> of account <b>{account}</b> as follows:
        <ul>
            <li>Make its parent be permission <b>{parent}</b>
            <li>The permission will be satisfied if the sum of weights from the following at least <b>{threshold}</b>:
                <table style="border: 1px solid black">
                    <tr><th>Type</th><th>Value</th><th>Weight</th></tr>
                    {keys}
                </table>
        </ul>
    )";

    result = result.replace("{permission}", action_data.permission);
    result = result.replace("{account}", action_data.account);
    result = result.replace("{parent}", action_data.parent);
    result = result.replace("{threshold}", action_data.auth.threshold);
    result = result.replace("{keys}", action_data.auth.keys);

    return result;
}
