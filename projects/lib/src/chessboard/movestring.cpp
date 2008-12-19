/*
    This file is part of SloppyGUI.

    SloppyGUI is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SloppyGUI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SloppyGUI.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "chessboard.h"
#include "notation.h"

using namespace Chess;


QString Board::moveString(const Move& move, MoveNotation notation)
{
	// Long Algebraic notation doesn't support castling in random
	// variants like Fischer Random chess, so we'll use SAN for
	// castling.
	if (notation == StandardAlgebraic
	||  (move.castlingSide() != -1 && m_isRandom))
		return sanMoveString(move);
	return longAlgebraicMoveString(move);
}

Move Board::moveFromString(const QString& str)
{
	Move move = moveFromSanString(str);
	if (move.sourceSquare() == 0 || move.targetSquare() == 0)
		move = moveFromLongAlgebraicString(str);
	return move;
}


QString Board::longAlgebraicMoveString(const Move& move) const
{
	Square source = chessSquare(move.sourceSquare());
	Square target = chessSquare(move.targetSquare());
	
	QString str = Notation::squareString(source) +
	              Notation::squareString(target);
	
	if (move.promotion() != NoPiece)
		str += Notation::pieceChar(move.promotion()).toLower();
	
	return str;
}

QString Board::sanMoveString(const Move& move)
{
	QString str;
	int source = move.sourceSquare();
	int target = move.targetSquare();
	int piece = m_squares[source] * m_sign;
	int capture = m_squares[target];
	Square square = chessSquare(source);
	
	char checkOrMate = 0;
	makeMove(move);
	if (inCheck(m_side)) {
		if (legalMoves().empty())
			checkOrMate = '#';
		else
			checkOrMate = '+';
	}
	undoMove();
	
	bool needRank = false;
	bool needFile = false;
	
	if (piece == Pawn) {
		if (target == m_enpassantSquare)
			capture = -Pawn * m_sign;
		if (capture != NoPiece)
			needFile = true;
	} else if (piece == King) {
		int cside = move.castlingSide();
		if (cside != -1) {
			if (cside == QueenSide)
				str = "O-O-O";
			else
				str = "O-O";
			if (checkOrMate != 0)
				str += checkOrMate;
			return str;
		} else
			str += Notation::pieceChar(piece);
	} else {
		str += Notation::pieceChar(piece);
		QVector<Move> moves = legalMoves();
		
		QVector<Move>::iterator it;
		for (it = moves.begin(); it != moves.end(); ++it) {
			int source2 = it->sourceSquare();
			if (source2 == source)
				continue;
			if ((m_squares[source2] * m_sign) != piece)
				continue;
			if (it->targetSquare() == target) {
				Square square2 = chessSquare(source2);
				if (square2.file != square.file)
					needFile = true;
				else if (square2.rank != square.rank)
					needRank = true;
			}
		}
	}
	if (needFile)
		str += 'a' + square.file;
	if (needRank)
		str += '1' + square.rank;

	if (capture != NoPiece)
		str += 'x';
	
	str += Notation::squareString(chessSquare(target));

	if (move.promotion()) {
		str += '=';
		str += Notation::pieceChar(move.promotion());
	}

	if (checkOrMate != 0)
		str += checkOrMate;
	
	return str;
}

Move Board::moveFromLongAlgebraicString(const QString& str) const
{
	if (str.length() < 4)
		return Move(0, 0);
	
	Square sourceSq = Notation::square(str.mid(0, 2));
	Square targetSq = Notation::square(str.mid(2, 2));
	if (!isValidSquare(sourceSq) || !isValidSquare(targetSq))
		return Move(0, 0);
	
	int promotion = NoPiece;
	if (str.length() > 4) {
		promotion = Notation::pieceCode(str[4].toUpper());
		if (promotion == NoPiece)
			return Move(0, 0);
	}
	
	int source = squareIndex(sourceSq);
	int target = squareIndex(targetSq);
	
	int castlingSide = -1;
	if ((m_squares[source] * m_sign) == King) {
		int diff = target - source;
		if (diff == -2 || diff == -3)
			castlingSide = QueenSide;
		else if (diff == 2 || diff == 3)
			castlingSide = KingSide;
	}
	
	return Move(source, target, promotion, castlingSide);
}

Move Board::moveFromSanString(const QString& str)
{
	if (str.length() < 2)
		return Move(0, 0);
	
	QString mstr = str;
	
	// Ignore check/mate/strong move/blunder notation
	while (mstr.endsWith('+') || mstr.endsWith('#')
	||     mstr.endsWith('!') || mstr.endsWith('?')) {
		mstr.chop(1);
	}
	
	if (mstr.length() < 2)
		return Move(0, 0);

	// Castling
	if (mstr.startsWith("O-O")) {
		int cside;
		if (mstr == "O-O")
			cside = KingSide;
		else if (mstr == "O-O-O")
			cside = QueenSide;
		else
			return Move(0, 0);
		
		int source = m_kingSquare[m_side];
		int target = m_castleTarget[m_side][cside];
		return Move(source, target, NoPiece, cside);
	}
	
	Square sourceSq = { -1, -1 };
	Square targetSq = { -1, -1 };
	QString::const_iterator it = mstr.begin();
	
	// A SAN move can't start with the capture mark, and
	// a pawn move must not specify the piece type
	if (*it == 'x' || *it == 'P')
		return Move(0, 0);
	
	// Piece type
	int piece = Notation::pieceCode(*it);
	if (piece < 0)
		piece = NoPiece;
	if (piece == NoPiece) {
		piece = Pawn;
		targetSq = Notation::square(mstr.mid(0, 2));
		if (isValidSquare(targetSq))
			it += 2;
	} else
		++it;
	
	bool stringIsCapture = false;
	
	if (!isValidSquare(targetSq)) {
		// Source square's file
		sourceSq.file = it->toAscii() - 'a';
		if (sourceSq.file < 0 || sourceSq.file >= m_width)
			sourceSq.file = -1;
		else if (++it == mstr.end())
			return Move(0, 0);

		// Source square's rank
		if (it->isDigit()) {
			sourceSq.rank = it->toAscii() - '1';
			if (sourceSq.rank < 0 || sourceSq.rank >= m_height)
				return Move(0, 0);
			++it;
		}
		if (it == mstr.end()) {
			// What we thought was the source square, was
			// actually the target square.
			if (isValidSquare(sourceSq)) {
				targetSq = sourceSq;
				sourceSq.rank = -1;
				sourceSq.file = -1;
			} else
				return Move(0, 0);
		}
		// Capture
		else if (*it == 'x') {
			if(++it == mstr.end())
				return Move(0, 0);
			stringIsCapture = true;
		}
		
		// Target square
		if (!isValidSquare(targetSq)) {
			if (it + 1 == mstr.end())
				return Move(0, 0);
			targetSq = Notation::square(mstr.mid(it - mstr.begin(), 2));
			it += 2;
		}
	}
	if (!isValidSquare(targetSq))
		return Move(0, 0);
	int target = squareIndex(targetSq);
	
	// Make sure that the move string is right about whether
	// or not the move is a capture.
	bool isCapture = false;
	if ((m_squares[target] * m_sign) < 0
	||  (target == m_enpassantSquare && piece == Pawn))
		isCapture = true;
	if (isCapture != stringIsCapture)
		return Move(0, 0);
	
	// Promotion
	int promotion = NoPiece;
	if (it != mstr.end())
	{
		if ((*it == '=' || *it == '(') && ++it == mstr.end())
			return Move(0, 0);
		
		promotion = Notation::pieceCode(*it);
		if (promotion == NoPiece)
			return Move(0, 0);
	}
	
	QVector<Move> moves = legalMoves();
	QVector<Move>::const_iterator move;
	QVector<Move>::const_iterator match = moves.end();
	
	// Loop through all legal moves to find a move that matches
	// the data we got from the move string.
	for (move = moves.begin(); move != moves.end(); ++move) {
		int piece2 = m_squares[move->sourceSquare()] * m_sign;
		if (piece2 != piece)
			continue;
		if (move->targetSquare() != target)
			continue;
		Square sourceSq2 = chessSquare(move->sourceSquare());
		if (sourceSq.rank != -1 && sourceSq2.rank != sourceSq.rank)
			continue;
		if (sourceSq.file != -1 && sourceSq2.file != sourceSq.file)
			continue;
		// Castling moves were handled earlier
		if (move->castlingSide() != -1)
			continue;
		if (move->promotion() != promotion)
			continue;
		
		// Return an empty move if there are multiple moves that
		// match the move string.
		if (match != moves.end())
			return Move(0, 0);
		match = move;
	}
	
	if (match != moves.end())
		return *match;
	
	return Move(0, 0);
}