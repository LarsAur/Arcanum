from chessboard import ChessBoard
from interface import Interface

MAX_TURNS = 200

class ChessGame():
    def __init__(self, engine1Path, engine2Path) -> None:
        self.board = ChessBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        
        self.iface = Interface(engine1Path, engine2Path)
        self.iface.start_uci()
        # self.iface.set_sf_elo(elo)              # Set stockfish elo
        self.iface.set_position_with_movelist() # Set start position
        self.uci_movelist = []
        self.result = "undefined"
    
    def play(self, time_ms):
        print(self.board.get_board_string())
    
        for _ in range(MAX_TURNS):
            game_status = self.iface.get_checkmate_status()
            if(game_status != 0):
                if(game_status == 1):
                    self.result = "stalemate"
                elif(game_status == 2):
                    self.result = "white"
                elif(game_status == 3):
                    self.result = "black"
                break
            
            move = self.iface.get_best_move_in_time(False, time_ms)
            self.uci_movelist.append(move)
            print(move)
            self.board.perform_move(move)
            print(self.board.get_board_string())
            self.iface.set_position_with_movelist(self.uci_movelist)
            
            game_status = self.iface.get_checkmate_status()
            if(game_status != 0):
                if(game_status == 1):
                    self.result = "stalemate"
                elif(game_status == 2):
                    self.result = "white"
                elif(game_status == 3):
                    self.result = "black"
                break
                    
            move = self.iface.get_best_move_in_time(True, time_ms)
            self.uci_movelist.append(move)
            print(move)
            self.board.perform_move(move)
            print(self.board.get_board_string())
            self.iface.set_position_with_movelist(self.uci_movelist)
            
    def save(self, filename):
        with open(filename, 'w+') as f:
            f.write("result: " + self.result + "\n")
            f.write(" ".join(self.uci_movelist))
            
    def get_result(self):
        return self.result