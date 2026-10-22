//go:build !windows

package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
)

func spawn(name string, args []string, dir string, env []string, logName string) int {
	out, _ := os.Create(filepath.Join(logsDir, logName+".log"))
	errf, _ := os.Create(filepath.Join(logsDir, logName+".err"))
	cmd := exec.Command(name, args...)
	cmd.Dir, cmd.Env, cmd.Stdout, cmd.Stderr = dir, env, out, errf
	cmd.SysProcAttr = &syscall.SysProcAttr{Setsid: true}
	if err := cmd.Start(); err != nil {
		die("failed to start %s: %v", logName, err)
	}
	return cmd.Process.Pid
}

func killTree(pid int) {
	// Negative pid signals the whole process group created by Setsid.
	syscall.Kill(-pid, syscall.SIGTERM)
}
