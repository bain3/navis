<h2 align="center">Navis - lightweight deployment server</h3>
Navis is a small deployment server meant to run behind some kind of a proxy. Its main job
is to receive deployment webhooks from github and to execute commands according to the
local configuration file. All that without being a burden on the system.

### Configuration and usage
You can try and download a binary release in releases, and potentially from the latest
deployment. You can then just simply run the binary:
```console
$ ./navis /path/to/config/file.json
[Sun Apr  1 04:20:00 1337][Info] Listening on port 42173
```
All configuration is done through a single json file.
```
{
  "host": "0.0.0.0", // network adapter (0.0.0.0 for everything)
  "public_hostname": "https://deploy.bain.cz", // used to construct URLs
  "port": 42173, // defines http port
  "repos": [
    {
      "name": "octocat/example", // full repo name
      "environments": {
        "production": {
          "command": "/path/to/production.sh",
          // output can be "file" (Navis automatically constructs the url to NAVIS_OUTPUTFILE)
          // or "url" (script writes the url to NAVIS_OUTPUTFILE, 
          // Navis then reads and deletes the file)
          "output": "file",
          // if the environment is a production environment, then github doesn't automatically
          // deactivate old deployments. If true, Navis will remember the latest active
          // deployment and will deactivate it after a new one has been created
          "auto_inactive": true
        },
        "dev": {
          // Before executing the script, Navis sets a few environment variables:
          // NAVIS_LOGFILE - Path to the log file
          // NAVIS_OUTPUTFILE - Path to the expected output file
          // NAVIS_REF - the ref that is being deployed
          // NAVIS_REPONAME - full name of the repository (octocat/example in this case)
          "command": "echo \"Development!\" >> $NAVIS_LOGFILE",
          // all other params (like output) are optional
        }
      },
      "secret": "secret defined in the github webhook",
      "token": "token used to access the github api"
    }
  ]
}
```
*Note: comments are not supported by json*

Configuration file is not actively refreshed so after changes Navis needs to be
restarted.

#### Logging and output
Logging and output is done through leaving the command output in a predefined file.
An example build script:
```shell
{
  # Make sure to change the directory! Making a variable for it is a good idea, too.
  # Navis checks the script's exit code. If it is non zero then it marks the build as failed,
  # doesn't publish the output, but does publish logs
  export ROOTPATH="/path/to/deployment/root/";
  cd ROOTPATH || {echo "Failed to change directory"; exit 1};
  # I highly recommend using docker, if it is possible, in deployment scripts
  # but for the sake of clarity...
  git reset --hard;
  git pull;
  ./configure;
  make;
  echo " - Built project";
  cp binary $NAVIS_OUTPUTFILE; # needs to have output defined as "file" in config
} >> $NAVIS_LOGFILE 
# ^^^^ redirects all output to the log file since there are no secrets in builds
echo "This is not gonna be logged, but it is gonna show up in the stdout of Navis";
```
Another example of a deployment script
```shell
# Change dir, exit and log if failed
export ROOTPATH="/path/to/deployment/root/";
cd ROOTPATH || {echo "Failed to change directory" >> $NAVIS_LOGFILE; exit 1};

git reset --hard;
echo "AAAh, maybe git leaks a secret! Fortunately we're not sending it to the log file...";
{
  git pull;
  echo " - Git operations complete";
  docker-compose up --build -d;
  echo "https://example.com/" >> $NAVIS_OUTPUTFILE; # needs to have output defined as "url" in config
  echo " - Deployed example.com!";
} >> $NAVIS_LOGFILE
```

#### Setting the webhook and sending deployment orders
To set a deployment webhook go to your repository's settings and add a new webhook pointing
to `https://your.navis.instance/deploy` (<-- endpoint: `/deploy`). The content type must be json, 
and Navis only supports deployment events.

Sending a deployment event is easy. There is an example python script that I use to deploy
my repos called `deploy.py`.

### Compiling
To compile Navis simply run `cmake` in the root directory of the project
```console
$ cmake .
...
-- Build files have been written to: /some/path
$ cmake --build .
[100%] Built target navis
```
The output should be a binary called navis(.exe) in the working directory.

#### Dependencies and copyright
All dependencies are already included in the `libs` folder.
(Apart from OpenSSL)

Navis is completely built on top of httplib, made by yhirose. Cryptographic functions
like hmac and sha1 are made by Stephan Brumme. JSON implementation is by nlohmann.

Navis' source is under the MIT license.
