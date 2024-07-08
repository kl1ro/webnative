start_app() {
	webpack --config webpack.common.js
	npx tailwindcss -i ./app/globals.css -o ./app/public/styles.css
	make
	./dist/linux64 & MAIN_PID=$!
  sleep 2
  CHILD_PID=$(pgrep -P $MAIN_PID)
}
stop_app() {
	echo $PID
	kill -9 $CHILD_PID
	kill -9 $MAIN_PID
	wait $MAIN_PID 2>/dev/null
	wait $CHILD_PID 2>/dev/null
}
start_app
while inotifywait -r -e modify,create,delete,move .; do
	stop_app
	start_app
done