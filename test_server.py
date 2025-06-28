"""
# OS Task 3 tests

## How to run

1. Install python 3.12 or above, here are some ways:

    - Use an installer (pyenv): https://github.com/pyenv/pyenv
    - Or manually compile (python.org): https://www.python.org/downloads/

2. Install requirements:

    - cd path/to/dir/containing/server.c
    - OPTIONAL: pyenv local 3.12     # If you used pyenv
    - python3 --version    # Make sure your version is 3.12 or above
    - python3 -m venv venv
    - source venv/bin/activate
    - python3 -m pip install pytest pytest-timeout httpx

3. Make two changes in your files:

    - In `output.c`, update `spinfor`:

        double spinfor = 0.5; // set this to 0.5

    - In `log.c`, add sleep **after write lock and before write release**:

        #include <unistd.h> // add this long for usleep

        void add_to_log(server_log log, const char* data, int data_len) {
            // ... acquire write lock ...

            // simulate long write time
            usleep(200000); // set this to 0.2 seconds (in microseconds)

            // ... release write lock ...
        }

    - Make sure in `test_server.py`:

        CGI_SPINFOR = 0.5    # matches content in `output.c`
        ADD_LOG_SLEEP = 0.2  # matches content in `log.c`

4. Compile your solution:

    - make clean
    - make

5. Run tests:

    - python3 -m pytest test_server.py -vv -s -x --ff --timeout 30

## Write your own tests

> Note: If you do add your own tests, please share them :)

The test framework automatically runs your server, sends requests, and parses the
response headers.

Each test is [parameterized](https://docs.pytest.org/en/stable/example/parametrize.html),
they values in the parameters array are expected to be a dictionary of type
`ResponseParams` (defined in this file, see its documentation).

What you get back is a `Responses` object (defined in this file, see its documentation).
It contains:
    - headers: Parsed headers of each response, **in the order you defined in params,
               which is not necessarily order sent or received**. Each header also
               has the original response object, see
               [`httpx.Response`](https://www.python-httpx.org/api/#response) for more.
    - params: The original `ResponseParams` used to send the requests.

To add a test, in the TestServer class, add a function like so:

```python
class TestServer:
    @pytest.mark.parametrize(
        "responses",
        [
            # first test
            dict(
                threads=1,  # threads to pass to server
                queue_size=1,  # queue_size to pass to server
                batches=[
                    [
                        dict(method="GET", url="home.html"),
                    ],
                ],
            ),
            # second test
            dict(
                threads=5,
                queue_size=5,
                batches=[
                    [*repeat(dict(method="GET", url="output.cgi"), 5)]
                ],
            ),
        ],
        indirect=["responses"],  # make sure to include this, for why see: https://docs.pytest.org/en/7.1.x/example/parametrize.html#indirect-parametrization
    )
    def test_your_custom_test(self, responses: Responses):
        headers = responses.headers

        for header in headers:
            print(header.StatThreadId, header.StatReqArrival, header.StatReqDispatch)
            print(header.response.text)

        assert False, "fill in test implementation"
```

## Help

Feel free to ask me (Tom Selfin) questions on whatsapp.

If a test fails "sometimes", try increasing the hard coded TestServer.STAGGER to 0.1.

If the tests aren't running please first run these commands:

```sh
uname -a  # for really weird kernel version bugs
pwd  # should be your project directory
ls  # make sure the "./server" binary is here, and "test_server.py"
which python3  # should be something like "*/venv/bin/python"
python3 --version  # should be 3.12 or above
python3 -m pip freeze | grep -E "(httpx|pytest)"  # should return 3 lines: httpx, pytest, and pytest-timeout
```

And attach their output.


Good luck and hopefully we will have quieter days :)

"""

import asyncio
import collections
import os
import pathlib
import subprocess as sp
import textwrap
import time
import typing
from itertools import repeat

import httpx
import pytest

CGI_SPINFOR = 0.5
ADD_LOG_SLEEP = 0.2


class ResponseStats(typing.NamedTuple):
    StatReqArrival: float
    StatReqDispatch: float
    StatThreadId: int
    StatThreadCount: int
    StatThreadStatic: int
    StatThreadDynamic: int
    StatThreadPost: int

    response: httpx.Response
    """TODO: debug - remove this"""

    def to_string(self) -> str:
        return (
            f"Stat-Req-Arrival:: {self.StatReqArrival:.6f}\r\n"
            f"Stat-Req-Dispatch:: {self.StatReqDispatch:.6f}\r\n"
            f"Stat-Thread-Id:: {self.StatThreadId}\r\n"
            f"Stat-Thread-Count:: {self.StatThreadCount}\r\n"
            f"Stat-Thread-Static:: {self.StatThreadStatic}\r\n"
            f"Stat-Thread-Dynamic:: {self.StatThreadDynamic}\r\n"
            f"Stat-Thread-Post:: {self.StatThreadPost}\r\n\r\n"
        )


class ResponseParams(typing.TypedDict):
    threads: int
    """threads to pass to server"""

    queue_size: int
    """queue_size to pass to server"""

    stagger: float | None
    """seconds to sleep between messages in each batch, None means no sleep"""

    batches: list[list[dict]]
    """
        **Batches** of requests to send to server.
        
        Between each **batch** the client will sleep for CGI_SPINFOR * 1.5 seconds.
        Between each **message** in each **batch** the client will sleep for `stagger`
        param seconds.

        A 'message' is a dictionary that will be passed to `httpx.request`, and supports
        the expected http parameters:
            - method: "GET", "POST", ...
            - url (defaults to ""): "home.html", "output.cgi", ...
            - timeout (defaults to None): seconds to raise timeout exception after
            - see all params here: https://www.python-httpx.org/api/#asyncclient (ctrl+f 'async request')
    """

    warmup: list[dict]
    """
    Messages to send **before** batches.
    
    NOTE: Responses will not be returned or validated.

    Can be used to help stabilize tests.
    """


class Responses(typing.NamedTuple):
    headers: list[ResponseStats]
    """parsed headers of each request **in order defined in params**"""

    params: ResponseParams
    """original params that were used to get the responses"""


class TestServer:
    STAGGER = 0.01

    def parse_headers(self, response: httpx.Response) -> ResponseStats:
        headers = response.headers
        return ResponseStats(
            StatReqArrival=float(headers["Stat-Req-Arrival"][2:]),
            StatReqDispatch=float(headers["Stat-Req-Dispatch"][2:]),
            StatThreadId=int(headers["Stat-Thread-Id"][2:]),
            StatThreadCount=int(headers["Stat-Thread-Count"][2:]),
            StatThreadStatic=int(headers["Stat-Thread-Static"][2:]),
            StatThreadDynamic=int(headers["Stat-Thread-Dynamic"][2:]),
            StatThreadPost=int(headers["Stat-Thread-Post"][2:]),
            response=response,
        )

    @pytest.fixture
    def responses(self, request) -> typing.Generator[Responses, None, None]:
        port = 8888

        params: ResponseParams = {
            "threads": request.param["threads"],
            "queue_size": request.param["queue_size"],
            "batches": request.param["batches"],
            "stagger": request.param.get("stagger", self.STAGGER),
            "warmup": request.param.get("warmup", []),
        }

        cmd = [
            *os.environ.get("SERVER_CMD", "./server").split(" "),
            str(port),
            str(params["threads"]),
            str(params["queue_size"]),
        ]
        print(f"Running cmd: {cmd}")

        proc = sp.Popen(
            cmd,
            cwd=pathlib.Path(__file__).parent,
        )

        time.sleep(0.5)
        try:
            headers = asyncio.run(self._send_cgi_requests(port, params))
            yield Responses(headers, params)
        finally:
            proc.kill()
            proc.wait()

    async def _send_cgi_requests(
        self, port: int, params: ResponseParams
    ) -> list[ResponseStats]:
        async with httpx.AsyncClient(base_url=f"http://localhost:{port}") as session:
            for msg in params["warmup"]:
                await session.request(method="POST", url="")

            requests = []
            for batch in params["batches"]:
                for msg in batch:
                    msg["url"] = msg.get("url", "")
                    msg["timeout"] = msg.get("timeout")
                    requests.append(asyncio.create_task(session.request(**msg)))
                    if params["stagger"] is not None:
                        await asyncio.sleep(params["stagger"])
                await asyncio.sleep(CGI_SPINFOR * 1.5)

            responses: list[httpx.Response] = [await r for r in requests]

        assert all(r.is_success for r in responses), [r.status_code for r in responses]

        return [self.parse_headers(r) for r in responses]

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=1,
                queue_size=5,
                batches=[
                    [dict(method="GET")],
                    [*repeat(dict(method="GET", url="output.cgi"), 5)],
                ],
            ),
            dict(
                threads=5,
                queue_size=1,
                batches=[
                    [dict(method="GET")],
                    [*repeat(dict(method="GET", url="output.cgi"), 5)],
                ],
            ),
            dict(
                threads=5,
                queue_size=5,
                batches=[
                    [dict(method="GET")],
                    [*repeat(dict(method="GET", url="output.cgi"), 5)],
                ],
            ),
            dict(
                threads=3,
                queue_size=3,
                batches=[
                    [dict(method="GET")],
                    [*repeat(dict(method="GET", url="output.cgi"), 5)],
                ],
            ),
        ],
        indirect=["responses"],
    )
    def test_responds_to_requests_in_order(self, responses: Responses):
        headers = responses.headers

        arrival = [h.StatReqArrival for h in headers]

        indices = list(range(len(headers)))
        sorted_arrival = list(sorted(zip(arrival, indices)))

        assert [s[1] for s in sorted_arrival] == indices, sorted_arrival

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=3,
                queue_size=3,
                batches=[repeat(dict(method="GET", url="output.cgi"), 3)],
            ),
            dict(
                threads=5,
                queue_size=5,
                batches=[repeat(dict(method="GET", url="output.cgi"), 5)],
            ),
        ],
        indirect=["responses"],
    )
    def test_processes_concurrent_requests(self, responses: Responses):
        for i, headers in enumerate(responses.headers):
            arrival = headers.StatReqArrival
            dispatch = headers.StatReqDispatch

            assert dispatch <= 0.1, f"Request {i} processed after {dispatch:.3f}s"

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=1,
                queue_size=5,
                batches=[[*repeat(dict(method="GET", url="home.html"), 5)]],
            ),
            dict(
                threads=1,
                queue_size=5,
                batches=[[*repeat(dict(method="GET", url="output.cgi"), 5)]],
            ),
            dict(
                threads=1,
                queue_size=5,
                batches=[[*repeat(dict(method="POST"), 5)]],
            ),
        ],
        indirect=["responses"],
    )
    def test_blocks_when_no_workers_available(self, responses: Responses):
        eps = CGI_SPINFOR / 2
        first_arrival = responses.headers[0].StatReqArrival

        arrivals = [h.StatReqArrival - first_arrival for h in responses.headers]
        dispatches = [h.StatReqDispatch for h in responses.headers]
        for i, h in enumerate(responses.headers):
            req = h.response.request

            process_time = 0
            if req.method == "GET":
                process_time += ADD_LOG_SLEEP

                if "cgi" in req.url.path:
                    process_time += CGI_SPINFOR

            arrival_diff = abs(self.STAGGER * i - arrivals[i])
            dispatch_diff = abs(process_time * i - dispatches[i])

            assert arrival_diff < eps, f"{i=} arrivals: {arrivals}"
            assert dispatch_diff < eps, f" {i=} dispatches: {dispatches}"

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=3,
                queue_size=5,
                stagger=CGI_SPINFOR / 3,
                batches=[[*repeat(dict(method="GET", url="output.cgi"), 17)]],
            ),
            dict(
                threads=2,
                queue_size=11,
                stagger=CGI_SPINFOR / 3,
                batches=[[*repeat(dict(method="GET", url="output.cgi"), 21)]],
            ),
            dict(
                threads=10,
                queue_size=10,
                batches=[[*repeat(dict(method="GET", url="output.cgi"), 7)]],
            ),
            dict(
                threads=4,
                queue_size=11,
                stagger=CGI_SPINFOR / 3,
                batches=[[*repeat(dict(method="GET", url="output.cgi"), 11)]],
            ),
        ],
        indirect=["responses"],
    )
    def test_splits_work_across_threads_round_robin(self, responses: Responses):
        """
        This test sends a lot of requests at once, and checks how many responses came
        from each thread. It expects work to be distributed **perfectly** between
        threads in a round robin fashion.

        It might be OK if it is not round robin, but check your distribution,
        does it look like work is being spread across threads?

        If you think this test should not pass, delete this function.
        Also, I (Tom Selfin) would like to know why you think so, since the staggering
        of messages should ensure round robin work distribution.
        """

        threads = responses.params["threads"]
        msgs = len(responses.params["batches"][0])
        base, extra = divmod(msgs, threads)

        expected_distribution = {base: threads - extra, base + 1: extra}
        # 0 count thread means no response sent, so not expected in distribution
        expected_distribution.pop(0, None)

        thread_counts = dict(
            (r.StatThreadId, r.StatThreadCount) for r in responses.headers
        ).values()
        actual_distribution = collections.Counter(thread_counts)

        assert len(responses.params["batches"][0]) == sum(thread_counts)

        assert expected_distribution == actual_distribution

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=3,
                queue_size=5,
                batches=[
                    [
                        *repeat(dict(method="GET", url="home.html"), 6),
                        *repeat(dict(method="GET", url="output.cgi"), 7),
                        *repeat(dict(method="POST"), 5),
                    ]
                ],
            ),
            dict(
                threads=10,
                queue_size=3,
                batches=[
                    [
                        *repeat(dict(method="GET", url="home.html"), 7),
                        *repeat(dict(method="GET", url="output.cgi"), 6),
                        *repeat(dict(method="POST"), 5),
                    ]
                ],
            ),
            dict(
                threads=7,
                queue_size=30,
                batches=[
                    [
                        *repeat(dict(method="GET", url="home.html"), 5),
                        *repeat(dict(method="GET", url="output.cgi"), 6),
                        *repeat(dict(method="POST"), 7),
                    ]
                ],
            ),
        ],
        indirect=["responses"],
    )
    def test_counts_request_types(self, responses: Responses):
        expected = collections.Counter(
            (
                "post"
                if msg["method"] == "POST"
                else "static" if msg["url"].endswith("html") else "dynamic"
            )
            for msg in responses.params["batches"][0]
        )

        actual_static = sum(
            dict(
                (r.StatThreadId, r.StatThreadStatic) for r in responses.headers
            ).values()
        )
        actual_dynamic = sum(
            dict(
                (r.StatThreadId, r.StatThreadDynamic) for r in responses.headers
            ).values()
        )
        actual_post = sum(
            dict((r.StatThreadId, r.StatThreadPost) for r in responses.headers).values()
        )

        actual_total_count = sum(
            dict(
                (r.StatThreadId, r.StatThreadCount) for r in responses.headers
            ).values()
        )

        assert expected["static"] == actual_static
        assert expected["dynamic"] == actual_dynamic
        assert expected["post"] == actual_post
        assert len(responses.params["batches"][0]) == actual_total_count

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=1,
                queue_size=1,
                batches=[[dict(method="GET", url="home.html")]],
            ),
        ],
        indirect=["responses"],
    )
    def test_thread_ids_start_at_1(self, responses: Responses):
        assert responses.headers[0].StatThreadId == 1

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=4,
                queue_size=3,
                batches=[repeat(dict(method="GET", url="output.cgi"), 10)],
            ),
            dict(
                threads=1,
                queue_size=2,
                batches=[repeat(dict(method="GET", url="output.cgi"), 5)],
            ),
            dict(
                threads=2,
                queue_size=1,
                batches=[repeat(dict(method="GET", url="output.cgi"), 5)],
            ),
            dict(
                threads=2,
                queue_size=2,
                batches=[repeat(dict(method="GET", url="output.cgi"), 5)],
            ),
        ],
        indirect=["responses"],
    )
    def test_pending_and_active_le_queue_size(self, responses: Responses):
        """
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/382_f2
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/373
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/mbqnl1buu9t7ei

        To summarize, this is the expected implementation:
            - Queue.wait
            - Accept
            - Arrival
            - Queue.enqueue
        """

        pending_and_active = []

        current = 0
        last_arrival = None
        arrivals = []
        for h in responses.headers:
            arrival = h.StatReqArrival
            arrivals.append(arrival)
            if last_arrival is None or arrival - last_arrival < CGI_SPINFOR / 2:
                current += 1
                last_arrival = arrival
            else:
                pending_and_active.append(current)
                current = 0
                last_arrival = None

        arrivals = [f"{a - arrivals[0]:.3f}" for a in arrivals]

        for concurrent in pending_and_active:
            assert concurrent <= responses.params["queue_size"], (
                responses.params["queue_size"],
                pending_and_active,
                arrivals,
            )

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=1,
                queue_size=9,
                warmup=[dict(method="POST")],
                batches=[
                    [
                        *repeat(dict(method="GET"), 4),
                        *repeat(dict(method="GET", url="output.cgi"), 4),
                        dict(method="POST"),
                    ]
                ],
            ),
            dict(
                threads=1,
                queue_size=1,
                warmup=[dict(method="POST")],
                batches=[
                    [
                        *repeat(dict(method="GET"), 4),
                        *repeat(dict(method="GET", url="output.cgi"), 4),
                        dict(method="POST"),
                    ]
                ],
            ),
        ],
        indirect=["responses"],
    )
    def test_log_serial_requests(self, responses: Responses):
        """
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/377
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/mbqnl1buu9t7ei

        Do we need to add a delimiter? **Yes**.

        The segel said the delimiter for the **last** log does not matter, but this
        test checks that it **does not exists**, e.g. stats1\nstats2\0
                                                                   ^^^ ends here

        If you have issues with this, feel free to tweak the test accordingly.
        """

        expected_log = "\n".join(h.to_string() for h in responses.headers[:-1])

        actual_log = responses.headers[-1].response.text

        assert expected_log == actual_log, (len(expected_log), len(actual_log))

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=5,
                queue_size=5,
                stagger=0.01,
                warmup=[dict(method="POST")],
                batches=[
                    [
                        dict(method="GET", url="home.html"),  # writer locks
                        *repeat(dict(method="POST"), 3),  # start readers
                        dict(method="GET", url="home.html"),  # start writer
                    ],
                    [
                        dict(method="POST"),  # final reader
                    ],
                ],
            ),
        ],
        indirect=["responses"],
    )
    def test_log_writer_locks_first_after_write_lock_released(
        self, responses: Responses
    ):
        """
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/377
        PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/mbqnl1buu9t7ei

        Do we need to add a delimiter? **Yes**

        The segel said the delimiter for the **last** log does not matter, but this
        test checks that it **does not exists**, e.g. stats1\nstats2\0
                                                                   ^^^ ends here

        If you have issues with this, feel free to tweak the test accordingly.
        """
        writer_0 = responses.headers[0]
        readers = responses.headers[1:-2]
        writer_1 = responses.headers[-2]
        final_reader = responses.headers[-1]

        expected_log = "\n".join((writer_0.to_string(), writer_1.to_string()))
        actual_log = final_reader.response.text

        for i, reader in enumerate(readers):
            assert reader.response.text == expected_log, f"Reader {i}"

        assert writer_0.to_string() in actual_log
        assert writer_1.to_string() in actual_log

        assert actual_log == expected_log

    @pytest.mark.parametrize(
        "responses",
        [
            dict(
                threads=5,
                queue_size=5,
                stagger=0.01,
                batches=[[dict(method="GET", url="output.cgi")]],
            ),
        ],
        indirect=["responses"],
    )
    @pytest.mark.xfail(
        reason=(
            "PIAZZA: https://piazza.com/class/m8nd0nnxsj77dt/post/456"
            " If option 1 is expected, this test should XPASS."
            " If option 2 is expected, this test should XFAIL."
        )
    )
    def test_headers(self, responses: Responses):
        """
        Depends on what they say here: https://piazza.com/class/m8nd0nnxsj77dt/post/456

        If option 1 is expected, this test should FAIL.
        If option 2 is expected, this test should PASS.
        """

        help_msg = r"""
        If this test fails, make sure your request server dynamic does no write
        "\r\n\r\n" before execv, since that means the CGI script will not be able to
        write it's own headers.
        """

        response = responses.headers[0].response
        assert "Content-Length" not in response.headers, (help_msg, response.headers)
