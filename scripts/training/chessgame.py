import chess.pgn
import chess.engine
import os
from chess.engine import Info
from typing import List
from typing import Tuple

class ChessGame():

    RESULT = [CHECKMATE, STALEMATE, DRAW50, DRAWREPEAT, DRAW, UNFINISHED] = ["checkmate", "stalemate", "draw 50", "draw repeat", "draw", "unfinished"]

    def __init__(self, white_engine_path:str, black_engine_path:str, fen:str="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", woptions:List[Tuple[str, str]]=[], boptions:List[Tuple[str, str]]=[]) -> None:
        self.board:chess.Board = chess.Board(fen)
        self.white_engine:chess.engine.SimpleEngine = chess.engine.SimpleEngine.popen_uci(white_engine_path)
        self.black_engine:chess.engine.SimpleEngine = chess.engine.SimpleEngine.popen_uci(black_engine_path)
        self.result = (ChessGame.UNFINISHED, None)
        self.mate_found = False

        for option in woptions:
            self.white_engine.configure({option[0] : option[1]})

        for option in boptions:
            self.black_engine.configure({option[0] : option[1]})

    def play_single_move(self, ms):
        results = None
        if self.board.turn == chess.WHITE:
            results = self.white_engine.play(self.board, chess.engine.Limit(time=ms/1000), info=Info.SCORE)
        else:
            results = self.black_engine.play(self.board, chess.engine.Limit(time=ms/1000), info=Info.SCORE)

        self.mate_found = results.info["score"].is_mate()
        move:chess.Move = results.move
        self.board.push(move)

    def is_mate_found(self):
        return self.mate_found

    def get_fen(self):
        return self.board.fen()

    def is_finished(self):
        return self.board.can_claim_draw() or self.board.is_stalemate() or self.board.is_checkmate()

    def terminate(self):
        self.white_engine.quit()
        self.black_engine.quit()
        self.white_engine.close()
        self.black_engine.close()

    def get_result(self):
        if(self.board.is_checkmate()):
            self.result = (ChessGame.CHECKMATE, not self.board.turn)
        elif(self.board.is_stalemate()):
            self.result = (ChessGame.STALEMATE, None)
        elif(self.board.can_claim_fifty_moves()):
            self.result = (ChessGame.DRAW50, None)
        elif(self.board.can_claim_threefold_repetition()):
            self.result = (ChessGame.DRAWREPEAT, None)
        elif(self.board.can_claim_draw()):
            self.result = (ChessGame.DRAW, None)

        return self.result