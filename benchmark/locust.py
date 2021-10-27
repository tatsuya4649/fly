from locust import events, HttpUser, task, between
import urllib3
from urllib3.exceptions import InsecureRequestWarning
urllib3.disable_warnings(InsecureRequestWarning)

class BenchUser(HttpUser):
    wait_time = between(0.1, 1)

    @task
    def index(self):
        self.client.get("/", verify=False)
