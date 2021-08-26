from fly import FlyRoute

print(dir(FlyRoute))

route = FlyRoute()
print(route._routes)

try:
	route._routes = 190
except Exception as e:
	print(e)

def a():
	print("hello")

route.register_route("/user", a, "get")
print(route._routes)
