# NC2022 project - Team 06 (Zagar & Madlener)
This project contains two main parts, NCM and Inference, located in `project/moep80211ncm` and `project/inference_pipeline` respectively.

## libmoep-ncm
The libmoep-ncm utilized for this project resides within the `project/moep80211ncm` folder. We adjusted the library to serve our LQE project purposes.
The functionality implemented can be controlled via the following parameters passed to the library at startup:
- Link Quality Statistics Collection (`-l <PORT>`):  This argument activates link quality statistics collection and transmits all formatted data through a TCP socket operating on localhost (the LQE backend) on a designated port (specified by the PORT argument). Most of the statistics are captured from the session or radiotap headers.
- Startup Connection Test (`-c <IP>`): When this flag is enabled, the libmoep-ncm library conducts a
connection test to the specified IP address (provided by the IP argument) during startup. (The `-l <PORT>` argument is required for this flag)
- Predicted Link Quality Estimation Reception from Backend (`-q`): This flag allows for receiving and printing predicted link quality estimations from the LQE backend. (The `-l <PORT>` argument is required for this flag)

## Inference Pipeline
The inference pipeline consists of three main parts (located in `project/inference_pipeline`). Training for initial model training with the Rutgers dataset. Server (and NCM Mock) for the actual inference pipeline, i.e. estimating Link qualities in real time.

### Training
- Python application for dataset pre-processing (synthetic feature generation) and decision tree training
- Located in `project/inference_pipeline/training/`

#### Run locally
If you want to train a decision tree with the Rutgers dataset do the following
- Make sure you have Docker installed on your machine (https://docs.docker.com/get-docker/)
- make sure you are in the folder `project/inference_pipeline/training/`
- `docker build -t training .`
- `docker run training`
After this is finished (may take up to a couple of minutes) a new dtree.joblib has been created inside the container.
Now you can replace the current dtree.joblib file of the Server with this new one. For this do the following
- `docker ps -a`
- Select and copy the container id of the finished container
- `docker cp <container id>:app/dtree.joblib ../server/app/`

### NCM Mock
- For local development and testing, a mock-NCM has been developed that sends mock data via sockets to the server and also listens for responses.
- Located in `project/inference_pipeline/ncm_mock/`

### Server (Inference)
FastAPI server which implements the inference pipeline and a web dashboard for monitoring

#### API
##### Access Web Dashboard
Endpoint for accessing the web dashboard
```http
GET http://localhost:8080/
```

#### Configuration
- If you want to replace the decision tree, you can replace the `dtree.joblib` file in `project/inference_pipeline/server/app/`. 

#### Run locally
We implemented a docker-compose.local.yml and a mock-NCM so you can run the inference pipeline also locally on your machine (not only the NCM)
- Make sure you have Docker installed on your machine (https://docs.docker.com/get-docker/)
- Change to `project/inference_pipeline/` and run `docker-compose -f docker-compose.local.yml up --build`
- Open http://localhost:8080/ in your browser

## Containerization and CI/CD pipeline
The entire software stack written in this project is containerized and is integrated, built and deployed via a CI/CD pipeline. Furthermore, the pipeline is able to execute certain commands in a tmux session to simplify development and demo effort (e.g. starting the libmoep-ncm library or establishing a connection between NCMs via the `ping` command).
The respective files for the containerization can be found in `project/docker-compose.yml` as well as the Dockerfiles in `project/moep80211ncm` and `project/inference_pipeline`.
The configuration file for the CI/CD pipeline can be found in `.github/workflows/main.yml`. Here one can adjust the hardware that is being deployed to as well as commands executed in tmux sessions etc.