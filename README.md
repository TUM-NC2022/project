# NC2022 project - Team 06 (Zagar & Madlener)
This project contains two main parts, NCM and Inference, located in `project/moep80211ncm` and `project/inference_pipeline` respectively.

## NCM
TODO

## Inference Pipeline
The inference pipeline consists of three main parts. Training for initial model training with the Rutgers dataset. Server (and NCM Mock) for the actual inference pipeline, i.e. estimating Link qualities in real time.

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
