// Cross-platform STMS launcher (Windows, Linux, macOS, Raspberry Pi). Standard library only.
//
// Usage: launch-stms [command] [--skip-native]
//
//	start    native build + Node deps + mobile backend and frontend (default)
//	stop     stop the mobile backend and frontend
//	setup    build the native Raspberry Pi tools
//	route | traffic | train   run a native pipeline step
//	cloud | jetson            run the optional Python integrations
//	all      run route, traffic and cloud
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"time"
)

const (
	backendPort  = 3000
	frontendPort = 8083
)

var (
	root        string
	backendDir  string
	frontendDir string
	logsDir     string
	pidFile     string
	nativeExe   string
	nativeBuild string
	isWin       = runtime.GOOS == "windows"
)

var nativeCommands = map[string]bool{
	"route":             true,
	"traffic":           true,
	"train":             true,
	"send-route":        true,
	"receive-jetson":    true,
	"receive-bluetooth": true,
}

var scripts = map[string][]string{
	"cloud": {"RaspberryPi/edge_pipeline.py", "--sync-cloud"},
}

func logf(format string, a ...any) { fmt.Printf(format+"\n", a...) }

func die(format string, a ...any) {
	fmt.Fprintf(os.Stderr, format+"\n", a...)
	os.Exit(1)
}

// findRoot walks up from the executable's folder, then the working directory,
// until it finds the project root.
func findRoot() string {
	var starts []string
	if exe, err := os.Executable(); err == nil {
		starts = append(starts, filepath.Dir(exe))
	}
	if wd, err := os.Getwd(); err == nil {
		starts = append(starts, wd)
	}
	for _, dir := range starts {
		for {
			if st, err := os.Stat(filepath.Join(dir, "RaspberryPi", "Native", "CMakeLists.txt")); err == nil && !st.IsDir() {
				return dir
			}
			parent := filepath.Dir(dir)
			if parent == dir {
				break
			}
			dir = parent
		}
	}
	die("Could not find the project root (a folder containing RaspberryPi/Native/CMakeLists.txt).")
	return ""
}

func exists(p string) bool { _, err := os.Stat(p); return err == nil }

func run(dir string, env []string, name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Dir = dir
	cmd.Stdout, cmd.Stderr, cmd.Stdin = os.Stdout, os.Stderr, os.Stdin
	if env != nil {
		cmd.Env = env
	}
	return cmd.Run()
}

func findCMake() string {
	if path, err := exec.LookPath("cmake"); err == nil {
		return path
	}
	if isWin {
		for _, path := range []string{
			`C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`,
			`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`,
			`C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`,
		} {
			if exists(path) {
				return path
			}
		}
	}
	return ""
}

// A cache created from a developer prompt pins the NMake/Ninja generator, which fails
// in a normal shell where those tools are not on PATH; drop it so CMake picks a default.
func resetStaleCache() {
	data, err := os.ReadFile(filepath.Join(nativeBuild, "CMakeCache.txt"))
	if err != nil {
		return
	}
	for _, line := range strings.Split(string(data), "\n") {
		if !strings.HasPrefix(line, "CMAKE_GENERATOR:INTERNAL=") {
			continue
		}
		gen := strings.TrimSpace(strings.TrimPrefix(line, "CMAKE_GENERATOR:INTERNAL="))
		tool := map[string]string{"NMake Makefiles": "nmake", "Ninja": "ninja"}[gen]
		if tool != "" {
			if _, err := exec.LookPath(tool); err != nil {
				logf("Discarding CMake cache generated with %s (%s not on PATH)", gen, tool)
				os.RemoveAll(nativeBuild)
			}
		}
	}
}

func setupNative() {
	cmake := findCMake()
	if cmake == "" {
		die("CMake was not found. Install CMake and a C++17 compiler.")
	}
	logf("== Native C++ build ==")
	resetStaleCache()
	if err := run(root, nil, cmake, "-S", filepath.Join(root, "RaspberryPi", "Native"),
		"-B", nativeBuild); err != nil {
		die("CMake configure failed: %v", err)
	}
	if err := run(root, nil, cmake, "--build", nativeBuild, "--config", "Release"); err != nil {
		die("Native build failed: %v", err)
	}
	if isWin {
		nativeExe = filepath.Join(nativeBuild, "Release", "stms-native.exe")
	} else {
		nativeExe = filepath.Join(nativeBuild, "stms-native")
	}
	if !exists(nativeExe) {
		die("Native executable was not produced at %s", nativeExe)
	}
}

func runNative(name string) int {
	if name == "jetson" {
		return runJetson()
	}
	executable := nativeExe
	if !exists(executable) {
		setupNative()
		executable = nativeExe
	}
	args := []string{name, "--root", root}
	if name == "receive-jetson" {
		args = append(args, "--output", filepath.Join(root, "jetson_data.jsonl"))
		if port := os.Getenv("STMS_JETSON_PORT"); port != "" {
			args = append(args, "--port", port)
		}
	}
	if name == "receive-bluetooth" {
		args = append(args, "--output", filepath.Join(root, "arduino_data.jsonl"))
		if port := os.Getenv("STMS_SENSOR_PORT"); port != "" {
			args = append(args, "--port", port)
		}
	}
	if err := run(root, nil, executable, args...); err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return ee.ExitCode()
		}
		return 1
	}
	return 0
}

func runJetson() int {
	executable := filepath.Join(root, "Jetson", "Cameras_nvidia_cuda", "build", "stms-jetson-cameras")
	config := os.Getenv("STMS_DEEPSTREAM_CONFIG")
	piHost := os.Getenv("STMS_PI_HOST")
	if isWin {
		executable += ".exe"
	}
	if !exists(executable) {
		fmt.Fprintf(os.Stderr, "Jetson camera target not found at %s. Build it on the Jetson with CMake.\n", executable)
		return 1
	}
	if config == "" {
		fmt.Fprintln(os.Stderr, "Set STMS_DEEPSTREAM_CONFIG to the DeepStream nvinfer configuration file.")
		return 1
	}
	trackerConfig := os.Getenv("STMS_TRACKER_CONFIG")
	if trackerConfig == "" {
		fmt.Fprintln(os.Stderr, "Set STMS_TRACKER_CONFIG to the DeepStream NvDCF tracker configuration file.")
		return 1
	}
	args := []string{"--config", config, "--tracker-config", trackerConfig}
	if trackerLibrary := os.Getenv("STMS_TRACKER_LIBRARY"); trackerLibrary != "" {
		args = append(args, "--tracker-lib", trackerLibrary)
	}
	if piHost != "" {
		args = append(args, "--pi", piHost)
	}
	if port := os.Getenv("STMS_PI_PORT"); port != "" {
		args = append(args, "--port", port)
	}
	if os.Getenv("STMS_JETSON_HEADLESS") == "1" {
		args = append(args, "--headless")
	}
	if err := run(root, nil, executable, args...); err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return ee.ExitCode()
		}
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	return 0
}

func runScript(name string) int {
	parts := scripts[name]
	python := filepath.Join(root, "PythonSetup", ".venv", "Scripts", "python.exe")
	if !isWin {
		python = filepath.Join(root, "PythonSetup", ".venv", "bin", "python")
	}
	if !exists(python) {
		for _, candidate := range []string{"python3", "python", "py"} {
			if path, err := exec.LookPath(candidate); err == nil {
				python = path
				break
			}
		}
	}
	if !exists(python) {
		if _, err := exec.LookPath(python); err != nil {
			fmt.Fprintln(os.Stderr, "Python is required for this optional integration. Install its dependencies from PythonSetup/requirements.txt.")
			return 1
		}
	}
	args := []string{}
	if filepath.Base(python) == "py" {
		args = append(args, "-3")
	}
	args = append(args, filepath.Join(root, filepath.FromSlash(parts[0])))
	args = append(args, parts[1:]...)
	if err := run(root, nil, python, args...); err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return ee.ExitCode()
		}
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	return 0
}

// findTool returns node/npm from PATH first, then common install folders.
func findTool(name string) string {
	if p, err := exec.LookPath(name); err == nil {
		return p
	}
	var candidates []string
	if isWin {
		exe := name + ".exe"
		if name == "npm" {
			exe = "npm.cmd"
		}
		for _, d := range []string{
			`C:\Program Files\nodejs`,
			`C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Microsoft\VisualStudio\NodeJs`,
			`C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Microsoft\VisualStudio\NodeJs`,
		} {
			candidates = append(candidates, filepath.Join(d, exe))
		}
	} else {
		for _, d := range []string{"/usr/local/bin", "/opt/homebrew/bin", "/usr/bin"} {
			candidates = append(candidates, filepath.Join(d, name))
		}
	}
	for _, c := range candidates {
		if exists(c) {
			return c
		}
	}
	return ""
}

// nodeEnv returns the environment with node's folder prepended to PATH.
func nodeEnv(node string, extra ...string) []string {
	env := os.Environ()
	for i, e := range env {
		if k, v, ok := strings.Cut(e, "="); ok && strings.EqualFold(k, "PATH") {
			env[i] = k + "=" + filepath.Dir(node) + string(os.PathListSeparator) + v
			return append(env, extra...)
		}
	}
	return append(append(env, "PATH="+filepath.Dir(node)), extra...)
}

func ensureNodeDeps(node, npm string) {
	logf("== Node dependencies ==")
	// Probe a known package so a half-installed tree is repaired too.
	for _, d := range []struct{ dir, probe string }{{backendDir, "express"}, {frontendDir, "write-file-atomic"}} {
		rel, _ := filepath.Rel(root, d.dir)
		if exists(filepath.Join(d.dir, "node_modules", d.probe)) {
			logf("%s: node_modules present", rel)
			continue
		}
		if npm == "" {
			die("node_modules missing in %s and npm was not found. Install Node.js.", rel)
		}
		logf("%s: running npm install", rel)
		if err := run(d.dir, nodeEnv(node), npm, "install", "--no-audit", "--no-fund"); err != nil {
			die("npm install failed in %s: %v", rel, err)
		}
	}
}

func portOpen(port int) bool {
	c, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", port), 500*time.Millisecond)
	if err != nil {
		return false
	}
	c.Close()
	return true
}

func waitHTTP(url string, timeout time.Duration) bool {
	client := &http.Client{Timeout: 5 * time.Second}
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if resp, err := client.Get(url); err == nil {
			resp.Body.Close()
			if resp.StatusCode >= 200 && resp.StatusCode < 400 {
				return true
			}
		}
		time.Sleep(time.Second)
	}
	return false
}

var scriptSrc = regexp.MustCompile(`<script[^>]+src="([^"]+)"`)

// warmBundle requests the JS bundle(s) the page references so Metro builds them
// before the user opens the app (otherwise the first load is a blank page).
func warmBundle(base string, timeout time.Duration) bool {
	page, err := (&http.Client{Timeout: 30 * time.Second}).Get(base)
	if err != nil {
		logf("Bundle warm-up error: %v", err)
		return false
	}
	html, _ := io.ReadAll(page.Body)
	page.Body.Close()
	client := &http.Client{Timeout: timeout}
	for _, m := range scriptSrc.FindAllStringSubmatch(string(html), -1) {
		url := m[1]
		if !strings.HasPrefix(url, "http") {
			url = strings.TrimRight(base, "/") + "/" + strings.TrimLeft(url, "/")
		}
		resp, err := client.Get(url)
		if err != nil {
			logf("Bundle warm-up error: %v", err)
			return false
		}
		io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
		if resp.StatusCode >= 400 {
			logf("Bundle warm-up error: HTTP %d", resp.StatusCode)
			return false
		}
	}
	return true
}

func startMobile() {
	logf("== Mobile app backend + frontend ==")
	node := findTool("node")
	if node == "" {
		die("Could not find node. Install Node.js (https://nodejs.org).")
	}
	ensureNodeDeps(node, findTool("npm"))
	os.MkdirAll(logsDir, 0o755)
	stop()
	for _, port := range []int{backendPort, frontendPort} {
		if portOpen(port) {
			die("Port %d is already in use by another process; free it and retry.", port)
		}
	}

	// CI=1 keeps Expo non-interactive.
	env := nodeEnv(node, "CI=1")
	backend := spawn(node, []string{"server.js"}, backendDir, env, "backend")
	frontend := spawn(node, []string{filepath.Join("node_modules", "expo", "bin", "cli"),
		"start", "--web", "--port", strconv.Itoa(frontendPort)}, frontendDir, env, "frontend")
	data, _ := json.Marshal(map[string]int{"backend": backend, "frontend": frontend})
	os.WriteFile(pidFile, data, 0o644)

	if !waitHTTP(fmt.Sprintf("http://localhost:%d/api/health", backendPort), 25*time.Second) {
		die("Backend did not become healthy; see %s", filepath.Join(logsDir, "backend.err"))
	}
	base := fmt.Sprintf("http://localhost:%d/", frontendPort)
	if !waitHTTP(base, 60*time.Second) {
		die("Frontend did not become ready; see %s", filepath.Join(logsDir, "frontend.err"))
	}
	logf("Building the web bundle (first start can take a minute or two)...")
	if !warmBundle(base, 300*time.Second) {
		die("Web bundle failed to build; see %s", filepath.Join(logsDir, "frontend.err"))
	}
	logf("\nSTMS app started successfully.")
	logf("Backend:  http://localhost:%d/", backendPort)
	logf("Frontend: %s", base)
	logf("Logs:     %s", logsDir)
	logf("Stop with: launch-stms stop")
}

func stop() {
	data, err := os.ReadFile(pidFile)
	if err != nil {
		logf("No running STMS processes recorded.")
	} else {
		var pids map[string]int
		if json.Unmarshal(data, &pids) == nil {
			for name, pid := range pids {
				killTree(pid)
				logf("Stopped %s (pid %d).", name, pid)
			}
		}
		os.Remove(pidFile)
	}
	logf("STMS app stopped.")
}

func status() {
	backendUp := waitHTTP(fmt.Sprintf("http://localhost:%d/api/health", backendPort), 2*time.Second)
	frontendUp := waitHTTP(fmt.Sprintf("http://localhost:%d/", frontendPort), 2*time.Second)
	state := func(up bool) string {
		if up {
			return "running"
		}
		return "NOT running"
	}
	logf("Backend  (http://localhost:%d/): %s", backendPort, state(backendUp))
	logf("Frontend (http://localhost:%d/): %s", frontendPort, state(frontendUp))
	if !backendUp || !frontendUp {
		logf("Start it with: launch-stms start")
		os.Exit(1)
	}
}

func main() {
	root = findRoot()
	nativeBuild = filepath.Join(root, "build", "native")
	if isWin {
		nativeExe = filepath.Join(nativeBuild, "Release", "stms-native.exe")
		if !exists(nativeExe) {
			nativeExe = filepath.Join(nativeBuild, "stms-native.exe")
		}
	} else {
		nativeExe = filepath.Join(nativeBuild, "stms-native")
	}
	mobile := filepath.Join(root, "MobileApp")
	backendDir = filepath.Join(mobile, "backend", "node-express")
	frontendDir = filepath.Join(mobile, "frontend")
	logsDir = filepath.Join(mobile, "logs")
	pidFile = filepath.Join(logsDir, "pids.json")

	command, skipNative := "start", false
	for _, a := range os.Args[1:] {
		switch a {
		case "--skip-native", "-skip-native", "--skip-python", "-skip-python":
			skipNative = true
		case "-h", "--help", "help":
			fmt.Println("Usage: launch-stms [start|stop|status|setup|route|traffic|train|send-route|receive-jetson|receive-bluetooth|jetson|cloud|all] [--skip-native]")
			return
		default:
			command = a
		}
	}

	switch command {
	case "start":
		if !skipNative {
			setupNative()
		}
		startMobile()
	case "stop":
		stop()
	case "status":
		status()
	case "setup":
		setupNative()
	case "all":
		setupNative()
		for _, step := range []string{"route", "traffic", "cloud"} {
			var code int
			if nativeCommands[step] {
				code = runNative(step)
			} else {
				code = runScript(step)
			}
			if code != 0 {
				os.Exit(code)
			}
		}
	default:
		if nativeCommands[command] {
			os.Exit(runNative(command))
		}
		if command == "jetson" {
			os.Exit(runJetson())
		}
		if _, ok := scripts[command]; !ok {
			die("Unknown command %q. Use: start|stop|status|setup|route|traffic|train|cloud|jetson|all", command)
		}
		os.Exit(runScript(command))
	}
}
