#!/bin/python3.TOKEN9
import sys
import requests

API_TOKEN = ""

if len(sys.argv) < 3:
    print("Bad usage. deploy <user/repo> <environment>")
    exit(1)

r = requests.post(
    f"https://api.github.com/repos/{sys.argv[1]}/deployments",
    headers={"Authorization": f"Bearer {API_TOKEN}"},
    json={"ref": "master", "environment": sys.argv[2]}
)

if r.ok:
    print("Deployment order was sent")
