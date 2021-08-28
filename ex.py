from fly import FlyRoute

print(dir(FlyRoute))

route = FlyRoute()
print(route._routes)

def a():
	print("hello")

route.register_route("/user", a, "get")
print(route._routes)
