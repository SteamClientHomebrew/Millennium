interface FileInfo {
	content: string;
	filePath: string;
	fileName: string;
}

type BufferEncoding = 'ascii' | 'utf8' | 'utf-8' | 'utf16le' | 'ucs2' | 'ucs-2' | 'base64' | 'latin1' | 'binary' | 'hex';

interface SingleFileExprProps {
	basePath?: string;
	encoding?: BufferEncoding;
}

interface MultiFileExprProps {
	basePath?: string;
	include?: string; // A regex pattern to include files
	encoding?: BufferEncoding;
}

/**
 * Create a compile time filesystem expression.
 * This function will evaluate a file path at compile time, and embed a files content statically into the bundle.
 */
declare const constSysfsExpr: {
	(fileName: string, props: SingleFileExprProps): FileInfo;
	(props: MultiFileExprProps): FileInfo[];
};

export { constSysfsExpr };
