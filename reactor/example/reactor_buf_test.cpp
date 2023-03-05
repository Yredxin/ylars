#include <reactor_buf.h>
#include <unistd.h>

int main()
{
	ylars::in_buf ibuf;
	ylars::out_buf obuf;
	ibuf.readfd(STDIN_FILENO);
	ibuf.pop(3);
	ibuf.adjust();
	obuf.send_data(ibuf.get_data(), ibuf.length());
	obuf.write2fd(STDOUT_FILENO);
	return 0;
}