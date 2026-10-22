//go:build windows

package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"syscall"
)

const (
	detachedProcess     = 0x08000000 // CREATE_NO_WINDOW: hidden console shared by children
	breakawayFromJob    = 0x01000000
	newProcessGroupFlag = syscall.CREATE_NEW_PROCESS_GROUP
)

func spawn(name string, args []string, dir string, env []string, logName string) int {
	out, _ := os.Create(filepath.Join(logsDir, logName+".log"))
	errf, _ := os.Create(filepath.Join(logsDir, logName+".err"))
	// Detach from the console and (when allowed) from the parent's job object, so the
	// servers keep running after the terminal that launched them is closed.
	for _, flags := range []uint32{newProcessGroupFlag | detachedProcess | breakawayFromJob, newProcessGroupFlag | detachedProcess} {
		cmd := exec.Command(name, args...)
		cmd.Dir, cmd.Env, cmd.Stdout, cmd.Stderr = dir, env, out, errf
		cmd.SysProcAttr = &syscall.SysProcAttr{CreationFlags: flags}
		if err := cmd.Start(); err == nil {
			return cmd.Process.Pid
		}
	}
	die("failed to start %s", logName)
	return 0
}

func killTree(pid int) {
	exec.Command("taskkill", "/PID", strconv.Itoa(pid), "/T", "/F").Run()
}
