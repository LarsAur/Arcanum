import chess.pgn
import chess.engine

MAX_TURNS = 200

class ChessGame():

    RESULT = [CHECKMATE, STALEMATE, DRAW, UNFINISHED] = ["checkmate", "stalemate", "draw", "unfinished"]

    def __init__(self, white_engine_path:str, black_engine_path:str, fen:str="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") -> None:
        self.board:chess.Board = chess.Board(fen)
        self.white_engine:chess.engine.SimpleEngine = chess.engine.SimpleEngine.popen_uci(white_engine_path)
        self.black_engine:chess.engine.SimpleEngine = chess.engine.SimpleEngine.popen_uci(black_engine_path)
        self.moves = []
        self.result = (ChessGame.UNFINISHED, None)

    def play(self, time_ms): 
        print(self.board)
        
        while(not (self.board.can_claim_draw() or self.board.is_stalemate() or self.board.is_checkmate())):
            results = None
            if self.board.turn == chess.WHITE:
                results = self.white_engine.analyse(self.board, chess.engine.Limit(time=time_ms/1000.0))
            else:
                results = self.black_engine.analyse(self.board, chess.engine.Limit(time=time_ms/1000.0))
            
            move:chess.Move = results["pv"][0]
            self.board.push(move)
            self.moves.append(move.uci())
            # print('\n')
            # print(self.board)
            
        if(self.board.is_checkmate()):
            self.result = (ChessGame.CHECKMATE, not self.board.turn)
        elif(self.board.is_stalemate()):
            self.result = (ChessGame.STALEMATE, None)
        elif(self.board.can_claim_draw()):
            self.result = (ChessGame.DRAW, None)

        self.white_engine.close()
        self.black_engine.close()

    def get_result(self):
        return self.result
            
    def get_moves(self):
        return self.moves