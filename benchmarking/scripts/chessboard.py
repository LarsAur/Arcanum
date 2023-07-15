
NO_PIECE = ' '
class ChessBoard():
    def __init__(self, fen:str) -> None:
        self.board = 64*[NO_PIECE]
        
        file = 0
        rank = 7
        fen_position = 0
        while not (file > 7 and rank == 0):
            c = fen[fen_position]
            fen_position += 1

            # -- Move to next rank
            if(c == '/'):
                file = 0
                rank -= 1
                continue

            if (c > '0' and c <= '8'):
                file += ord(c) - ord('0')
                continue

            square = ((rank << 3) | file);
            file += 1
            self.board[square] = fen[fen_position-1]
            
            
    def get_board_string(self):
        build_str = ""
        for y in range(7,-1,-1):
            for x in range(8):
                index = y * 8 + x
                if(self.board[index] == NO_PIECE):
                    if (x + y) % 2 == 1:
                        build_str += ". "
                    else:
                        build_str += "  "
                else:
                    build_str += self.board[index] + " "
            build_str += "\n"
        return build_str
    
    def perform_move(self, uci_move):
        frm = ChessBoard.arithmetic_square_to_index(uci_move[0:2])
        to = ChessBoard.arithmetic_square_to_index(uci_move[2:4])
        
        # Verify that from is not en empty piece
        if(self.board[frm] == NO_PIECE):
            print("Error: Moved from empty square", uci_move)
            exit(-1)
        
        # Handle potential Castle
        if(self.board[frm].lower() == 'k' and to == frm + 2):
            self.board[frm + 1] = self.board[to + 1]
            self.board[to + 1] = NO_PIECE
        elif(self.board[frm].lower() == 'k' and to == frm - 2):
            self.board[frm - 1] = self.board[to - 2]
            self.board[to - 2] = NO_PIECE
        
        # Handle potential enpassant
        if(self.board[frm].lower() == 'p' and self.board[to] == NO_PIECE and abs(frm - to) % 8 != 0):
            if self.board[frm] == 'P':
                self.board[to - 8] = NO_PIECE
            else:
                self.board[to + 8] = NO_PIECE
                
        # Handle potential propotion
        if(self.board[frm].lower() == 'p' and ((to >> 3) == 7 or (to >> 3) == 0)):
            if self.board[frm] == 'P':
                self.board[to] = uci_move[4].upper()
            else:
                self.board[to] = uci_move[4].lower()
        else:
            self.board[to] = self.board[frm] 
            
        self.board[frm] = NO_PIECE
        
    def arithmetic_square_to_index(arithmetic_square):
        x = ord(arithmetic_square[0]) - ord('a')
        y = ord(arithmetic_square[1]) - ord('1')
        index = y * 8 + x
        return index