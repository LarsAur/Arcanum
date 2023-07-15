from subprocess import Popen, PIPE, STDOUT
import os
import sys
import time
import io

class Interface():
    def __init__(self, engine1Path, engine2Path) -> None:
        self.ce2_process = Popen([engine1Path], stdout=PIPE, stdin=PIPE, stderr=STDOUT)
        self.sf_process = Popen([engine2Path], stdout=PIPE, stdin=PIPE, stderr=STDOUT)

    def start_uci(self):
        self.ce2_process.stdin.write(b"uci\n")
        self.ce2_process.stdin.flush()
        self.sf_process.stdin.write(b"uci\n")
        self.sf_process.stdin.flush()
        
        while(True):
            out = self.sf_process.stdout.readline()
            print(bytes.decode(out, 'utf-8'), end='')
            if bytes.decode(out, 'utf-8') == "uciok\r\n":
                break
        
        while(True):
            out = self.ce2_process.stdout.readline()
            print(bytes.decode(out, 'utf-8'), end='')
            if bytes.decode(out, 'utf-8') == "uciok\r\n":
                break
        
    def set_sf_elo(self, elo):
        if(elo < 1350 or elo > 2850):
            print("Elo out of range")
            return
        
        elo_str = f"setoption name UCI_Elo value {elo}\n"
        self.sf_process.stdin.write(b"setoption name UCI_LimitStrength value true\n")
        self.sf_process.stdin.write(elo_str.encode('utf-8'))
        self.sf_process.stdin.flush()

    def set_sf_unlimited_elo(self):
        self.sf_process.stdin.write(b"option UCI_LimitStrength false")
        
    def set_position_with_movelist(self, uci_movelist = []):
        build_str = "position startpos"
        
        if(len(uci_movelist) > 0):
            build_str += " moves "
            build_str += " ".join(uci_movelist)
        build_str += "\n"
        
        self.sf_process.stdin.write(build_str.encode('utf-8'))
        self.sf_process.stdin.flush()
        self.ce2_process.stdin.write(build_str.encode('utf-8'))
        self.ce2_process.stdin.flush()
        
    def get_best_move_in_time(self, use_ce2, time_ms):
        build_str = f"go movetime {time_ms}\n"
        
        out = ""
        if use_ce2:
            self.ce2_process.stdin.write(build_str.encode('utf-8'))
            self.ce2_process.stdin.flush()
            time.sleep(time_ms / 1000)
            while(True):
                out = bytes.decode(self.ce2_process.stdout.readline(), 'utf-8')
                if(out.startswith("bestmove")):
                    return out.split(" ")[1].rstrip() # Remove the newline and carrage return
        else:
            self.sf_process.stdin.write(build_str.encode('utf-8'))
            self.sf_process.stdin.flush()
            time.sleep(time_ms / 1000)
            while(True):
                out = bytes.decode(self.sf_process.stdout.readline(), 'utf-8')
                if(out.startswith("bestmove")):
                    return out.split(" ")[1].rstrip()
                
    def get_checkmate_status(self):
        self.ce2_process.stdin.write(b"ischeckmate\n")
        self.ce2_process.stdin.flush()
        
        out = bytes.decode(self.ce2_process.stdout.readline(), 'utf-8')
        if(out.startswith("nocheckmate")):
            return 0
        if(out.startswith("stalemate")):
            return 1
        if(out.startswith("checkmate")):
            if(out.split(" ")[1].startswith("white")):
                return 2
            else:
                return 3 
        
    