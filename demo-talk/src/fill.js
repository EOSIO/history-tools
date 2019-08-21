const { execSync } = require('child_process');

const users = [
    'adam',
    'bill',
    'bob',
    'jack',
    'jane',
    'jenn',
    'jill',
    'joe',
    'sam',
    'sue',
];

try {
    execSync('cleos wallet unlock --password </password');
}
catch (e) {

}

function rnd(max) {
    return Math.floor(Math.random() * Math.floor(max));
}

const first = 100000;
let id = first;

while (true) {
    const replyTo = rnd(id - first);
    const user = users[rnd(users.length)];
    const action = [id, replyTo ? replyTo + first : 0, user, 'A post'];
    execSync("cleos push action talk post '" + JSON.stringify(action) + "' -p " + user);
    ++id;
}
