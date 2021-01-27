#!/usr/bin/env python

import configparser
import os
import re
import requests
import shutil
import subprocess
import sys
import tarfile

def is_tag(ref):
    regex = re.compile('v[0-9]+\.[0-9]+\..*')
    return regex.match(ref)

def get_commit_for_branch(branch):
    commit = None
    r = requests.get('https://api.github.com/repos/EOSIO/eos/git/refs/heads/{}'.format(branch))
    if r.status_code == 200:
        commit = r.json().get('object').get('sha')

    return commit

def get_commit_for_tag(tag):
    commit = None
    r = requests.get('https://api.github.com/repos/EOSIO/eos/git/refs/tags/{}'.format(tag))
    if r.status_code == 200:
        commit = r.json().get('object').get('sha')

    return commit

def sanitize_label(label):
    invalid = [' ', '..', '.', '/', '~', '=']
    for c in invalid:
        label = label.replace(c, '-')

    return label.strip('-')

CURRENT_COMMIT = get_commit_for_branch('develop')

BK_TOKEN = os.environ.get('BUILDKITE_API_KEY')
if not BK_TOKEN:
    sys.exit('Buildkite token not set')
headers = {
    'Authorization': 'Bearer {}'.format(BK_TOKEN)
}

try:
    shutil.rmtree('builds')
except OSError:
    pass
os.mkdir('builds')

existing_build_found = False
if CURRENT_COMMIT:
    print 'Attempting to get build directory for this branch ({})...'.format(CURRENT_COMMIT)
    r = requests.get('https://api.buildkite.com/v2/organizations/EOSIO/pipelines/eosio/builds?commit={}'.format(CURRENT_COMMIT), headers=headers)
    if r.status_code == 200:
        resp = r.json()
        if resp:
            for build in resp:
                for job in build.get('jobs'):
                    job_name = job.get('name')
                    if job_name and re.search(r":ubuntu:.+18.04.+Build$", job_name):
                        dir_r = requests.get(job.get('artifacts_url'), headers=headers)
                        if dir_r.status_code == 200:
                            download_url = dir_r.json().pop().get('download_url')
                            if download_url:
                                dl_r = requests.get(download_url, headers=headers)
                                open('current_build.tar.gz', 'wb').write(dl_r.content)
                                tar = tarfile.open('current_build.tar.gz')
                                tar.extractall(path='builds/current')
                                tar.close()
                                os.remove('current_build.tar.gz')
                                existing_build_found = True
        else:
            print 'No builds found for this branch ({})'.format(CURRENT_COMMIT)

print existing_build_found