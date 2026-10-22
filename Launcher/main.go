// Cross-platform STMS launcher (Windows, Linux, macOS, Raspberry Pi). Standard library only.
//
// Usage: launch-stms [command] [--skip-python]
//
//	start    Python setup + Node deps + mobile backend and frontend (default)
//	stop     stop the mobile backend and frontend
//	setup    only create the Python venv and install requirements
//	route | traffic | train | cloud | jetson   run a Python pipeline step
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
	setupDir    string
	venvDir     string
	backendDir  string
	frontendDir string
	logsDir     string
	pidFile     string
	isWin       = runtime.GOOS == "windows"
)

var scripts = map[string][]string{
	"route":   {"RaspberryPi/AI_routing_algorithm/ai_routing.py"},
	"traffic": {"RaspberryPi/Traffic_light_commands/main.py"},
	"train":   {"RaspberryPi/Traffic_light_commands/train.py"},
	"cloud":   {"RaspberryPi/edge_pipeline.py", "--sync-cloud"},
	"jetson":  {"RaspberryPi/edge_pipeline.py", "--jetson-cameras"},
}

func logf(format string, a ...any) { fmt.Printf(format+"\n", a...) }

func die(format string, a ...any) {
	fmt.Fprintf(os.Stderr, format+"\n", a...)
	os.Exit(1)
}

// findRoot walks up from the executable's folder, then the working directory,
// until it finds the project root (the folder containing PythonSetup).
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
			if st, err := os.Stat(filepath.Join(dir, "PythonSetup")); err == nil && st.IsDir() {
				return dir
			}
			parent := filepath.Dir(dir)
			if parent == dir {
				break
			}
			dir = parent
		}
	}
	die("Could not find the project root (a folder containing PythonSetup).")
	return ""
}

func exists(p string) bool { _, err := os.Stat(p); return err == nil }

func venvPython() string {
	if isWin {
		return filepath.Join(venvDir, "Scripts", "python.exe")
	}
	return filepath.Join(venvDir, "bin", "python")
}

func run(dir string, env []string, name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Dir = dir
	cmd.Stdout, cmd.Stderr, cmd.Stdin = os.Stdout, os.Stderr, os.Stdin
	if env != nil {
		cmd.Env = env
	}
	return cmd.Run()
}

func systemPython() (string, []string) {
	for _, n := range []string{"python3", "python", "py"} {
		p, err := exec.LookPath(n)
		if err != nil {
			continue
		}
		args := []string{}
		if n == "py" {
			args = []string{"-3"}
		}
		// Rejects the Windows Store stub, which is on PATH but not a real interpreter.
		if exec.Command(p, append(args, "--version")...).Run() != nil {
			continue
		}
		return p, args
	}
	die("Python 3 was not found. Install it from https://www.python.org.")
	return "", nil
}

func setupPython() string {
	logf("== Python setup ==")
	if !exists(venvPython()) {
		logf("Creating virtual environment in %s", venvDir)
		p, pre := systemPython()
		if err := run(root, nil, p, append(pre, "-m", "venv", venvDir)...); err != nil {
			die("venv creation failed: %v", err)
		}
	}
	py := venvPython()
	if err := run(root, nil, py, "-m", "pip", "install", "--quiet", "--upgrade", "pip", "setuptools", "wheel"); err != nil {
		die("pip upgrade failed: %v", err)
	}
	req := filepath.Join(setupDir, "requirements.txt")
	if exists(req) {
		if err := run(root, nil, py, "-m", "pip", "install", "--quiet", "-r", req); err != nil {
			die("pip install failed: %v", err)
		}
	}
	logf("Python environment ready: %s", py)
	return py
}

func runScript(name string) int {
	py := venvPython()
	if !exists(py) {
		py = setupPython()
	}
	parts := scripts[name]
	args := append([]string{filepath.Join(root, filepath.FromSlash(parts[0]))}, parts[1:]...)
	if err := run(root, nil, py, args...); err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return ee.ExitCode()
		}
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
	setupDir = filepath.Join(root, "PythonSetup")
	venvDir = filepath.Join(setupDir, ".venv")
	mobile := filepath.Join(root, "MobileApp")
	backendDir = filepath.Join(mobile, "backend", "node-express")
	frontendDir = filepath.Join(mobile, "frontend")
	logsDir = filepath.Join(mobile, "logs")
	pidFile = filepath.Join(logsDir, "pids.json")

	command, skipPython := "start", false
	for _, a := range os.Args[1:] {
		switch a {
		case "--skip-python", "-skip-python":
			skipPython = true
		case "-h", "--help", "help":
			fmt.Println("Usage: launch-stms [start|stop|status|setup|route|traffic|train|cloud|jetson|all] [--skip-python]")
			return
		default:
			command = a
		}
	}

	switch command {
	case "start":
		if !skipPython {
			setupPython()
		}
		startMobile()
	case "stop":
		stop()
	case "status":
		status()
	case "setup":
		setupPython()
	case "all":
		setupPython()
		for _, step := range []string{"route", "traffic", "cloud"} {
			if code := runScript(step); code != 0 {
				os.Exit(code)
			}
		}
	default:
		if _, ok := scripts[command]; !ok {
			die("Unknown command %q. Use: start|stop|status|setup|route|traffic|train|cloud|jetson|all", command)
		}
		os.Exit(runScript(command))
	}
}
