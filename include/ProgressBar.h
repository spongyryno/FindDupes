class ProgressBarImpl;

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class QPTimer
{
public:
	QPTimer()
	{
		QueryPerformanceFrequency(&m_freq);
		m_invfreq = 1.0 / ((double)m_freq.QuadPart);

		Restart();
	}

	~QPTimer()
	{
	}

	void Restart()
	{
		QueryPerformanceCounter(&m_start);
	}

	double GetDurationInSeconds() const
	{
		return Elapsed();
	}

	double Elapsed() const
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		double seconds = (double)(now.QuadPart - m_start.QuadPart) * m_invfreq;
		return seconds;
	}

private:
	LARGE_INTEGER	m_freq;
	LARGE_INTEGER	m_start;
	double			m_invfreq;
};


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class ProgressBar
{
public:
	ProgressBar();
	~ProgressBar();
	void Update(int dist, int total);
	void Update(long dist, long total);
	void Update(long long dist, long long total);
	void Update(double pct);

private:
	std::unique_ptr<ProgressBarImpl>	_;
};


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
extern size_t ___i; // for debugging
