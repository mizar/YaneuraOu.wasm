package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/schollz/progressbar"
)

type Options struct {
	HashSize        int
	PostSearchCount int
	DepthLimit      int
	TimeLimit       int
	OutFile         string
	Process         int
}

func parseOptions() Options {
	hash_size := flag.Int("hash", 64, "the size of hash (MB)")
	post_search_count := flag.Int("post-search-count", 0, "the number of post-search moves")
	depth_limit := flag.Int("mate-limit", 0, "the maximum mate length")
	time_limit := flag.Int("time-limit", 0, "the maximum time (msec)")
	out_file := flag.String("out", "", "the output file")
	num_process := flag.Int("process", 4, "the number of process")
	flag.Parse()

	return Options{
		HashSize:        *hash_size,
		PostSearchCount: *post_search_count,
		DepthLimit:      *depth_limit,
		TimeLimit:       *time_limit,
		OutFile:         *out_file,
		Process:         *num_process,
	}
}

type EngineProcess struct {
	cmd     *exec.Cmd
	stdin   io.WriteCloser
	stdout  io.ReadCloser
	scanner *bufio.Scanner
}

func newEngineProcess(command string) (*EngineProcess, error) {
	cmd := exec.Command(command)
	stdin, err := cmd.StdinPipe()
	if err != nil {
		return nil, err
	}

	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}
	scanner := bufio.NewScanner(stdout)

	err = cmd.Start()
	if err != nil {
		return nil, err
	}

	return &EngineProcess{cmd, stdin, stdout, scanner}, nil
}

func (ep *EngineProcess) SetOption(op Options) {
	fmt.Fprintf(ep.stdin, "setoption name USI_Hash value %d\n", op.HashSize)
	fmt.Fprintf(ep.stdin, "setoption name PostSearchCount value %d\n", op.PostSearchCount)
	fmt.Fprintf(ep.stdin, "setoption name DepthLimit value %d\n", op.DepthLimit)
	fmt.Fprintf(ep.stdin, "setoption name RootIsAndNodeIfChecked value false\n")
}

func (ep *EngineProcess) Ready() error {
	fmt.Fprintln(ep.stdin, "isready")

	for ep.scanner.Scan() {
		text := ep.scanner.Text()
		if strings.Contains(text, "readyok") {
			return nil
		}
	}

	err := ep.scanner.Err()
	if err != nil {
		return err
	}

	return fmt.Errorf("got no \"readyok\"")
}

func (ep *EngineProcess) solveImpl(sfen string) (int, error) {
	fmt.Fprintf(ep.stdin, "sfen %s\n", sfen)
	fmt.Fprintln(ep.stdin, "go mate 10000")

	r := regexp.MustCompile(`.*num_searched=(\d+).*`)
	num_searched := 0

	for ep.scanner.Scan() {
		text := ep.scanner.Text()
		switch {
		case r.MatchString(text):
			num, err := strconv.Atoi(r.FindStringSubmatch(text)[1])
			if err == nil {
				num_searched = num
			}
		case strings.Contains(text, "nomate"):
			return num_searched, fmt.Errorf("got nomate")
		case strings.Contains(text, "checkmate "):
			if text == "checkmate " {
				return num_searched, fmt.Errorf("got checkout without mate moves")
			} else {
				return num_searched, nil
			}
		}
	}
	err := ep.scanner.Err()
	if err != nil {
		return num_searched, err
	}

	return 0, fmt.Errorf("unexpected EOF")
}

func (ep *EngineProcess) Solve(sfen string, time_limit_ms int) (int, error) {
	if time_limit_ms == 0 {
		return ep.solveImpl(sfen)
	}

	timer := time.NewTimer(time.Duration(time_limit_ms) * time.Millisecond)
	result := make(chan struct {
		int
		error
	})
	go func() {
		val, err := ep.solveImpl(sfen)
		result <- struct {
			int
			error
		}{val, err}
	}()

	select {
	case <-timer.C:
		fmt.Fprintln(ep.stdin, "stop")
		<-result
		return 0, fmt.Errorf("time limit exceeded")
	case res := <-result:
		return res.int, res.error
	}
}

func solve(
	wg *sync.WaitGroup,
	bar *progressbar.ProgressBar,
	command string, op Options,
	sfen_input chan string,
	output_ch chan string) {
	defer wg.Done()

	process, err := newEngineProcess(command)
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(2)
	}
	process.SetOption(op)
	err = process.Ready()
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(2)
	}

	for sfen := range sfen_input {
		_, err := process.Solve(sfen, op.TimeLimit)
		if err != nil {
			output_ch <- fmt.Sprintf("%v: sfen %v", err, sfen)
		}
		bar.Add(1)
	}
}

func main() {
	op := parseOptions()

	if flag.NArg() == 0 {
		fmt.Println("error: solver command was not specified")
		flag.Usage()
		os.Exit(2)
	}

	bar := progressbar.Default(-1)

	command := flag.Arg(0)
	sfen_chan := make(chan string)
	output_chan := make(chan string)
	var wg sync.WaitGroup
	for i := 0; i < op.Process; i++ {
		wg.Add(1)
		go solve(&wg, bar, command, op, sfen_chan, output_chan)
	}

	end := make(chan struct{}, 1)
	var end_wg sync.WaitGroup
	go func() {
		defer end_wg.Done()
		end_wg.Add(1)

		has_outfile := false
		var outfile *os.File
		defer outfile.Close()
		if op.OutFile != "" {
			file, err := os.Create(op.OutFile)
			if err == nil {
				has_outfile = true
				outfile = file
			}
		}
		for {
			select {
			case out := <-output_chan:
				fmt.Printf("\r%v\n", out)
				if has_outfile {
					fmt.Fprintf(outfile, "\r%v\n", out)
				}
			case <-end:
				return
			}
		}
	}()

	sfen_scanner := bufio.NewScanner(os.Stdin)
	for sfen_scanner.Scan() {
		sfen := sfen_scanner.Text()
		sfen_chan <- sfen
	}
	close(sfen_chan)

	wg.Wait()

	end <- struct{}{}
	end_wg.Wait()
}
